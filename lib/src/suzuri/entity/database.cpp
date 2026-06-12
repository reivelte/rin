// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <format>
#include <limits>
#include "../utility/string.hpp"
#include "../utility/files.hpp"
#include "../exception.hpp"
#include "database.hpp"

namespace sz
{
#define THROW_IF_STEP_NOT_DONE(X) if (X.step() != sqlite::status_code::Done) throw exception(result_code::No_Action, m_db.error_message())
    /* public */

    entity_database::entity_database() :
        m_db(), m_read_only(true), m_use_aliases(false), m_recurse(true)
    {
    }

    entity_database::entity_database(const std::filesystem::path& path, bool read_only) :
        m_db(path), m_read_only(read_only), m_use_aliases(false), m_recurse(true)
    {
        if (m_read_only)
        { return; }
        
        m_init();
    }

    void entity_database::set_database(const std::filesystem::path& path)
    {
        m_db.connect(path);
        
        if (m_read_only)
        { return; }

        m_init();
    }

    bool entity_database::entity_exists(const std::string& id)
    {
        auto& q = m_db.query("SELECT EXISTS(SELECT 1 FROM entity WHERE id = ?)", id);
        q.restep();
        return q.column<int>() == 1;
    }

    entity_database_info entity_database::entity_info(const std::string& id)
    {
        if (!entity_exists(id))
        { throw exception(result_code::Invalid_Argument, std::format("'{}' does not refer to an entity in the database", id)); }
        
        // type,create_time,mod_time,rgba,id,description
        auto [t, create, mod, rgba, eid, description] = m_entity_info(id);
        auto type = entity_type{t};
        std::vector<sqlite::variant> extra_info;

        using enum entity_type;
        switch (type)
        {
        case File:
            extra_info = m_file_info(eid);
            break;
        
        case Alias:
            extra_info.emplace_back(alias_target(eid));
            break;

        default:
            break;
        }

        return entity_database_info{
            .extra_info = extra_info,
            .id = eid,
            .description = description,
            .createtime = create,
            .modtime = mod,
            .rgba = rgba,
            .type = type
        };
    }

    // TODO*: create a manual transaction instead of invoking exec for each statement, otherwise rollback wont do what we want
    // returns invalid_input if e.id is invalid (contains invalid characters) for the given entity type and redundant_action if the entity exists (skip_exist_check being false)
    result_code entity_database::register_entity(const entity_database_info& e) 
    {
        if (entity_exists(e.id))
        { return result_code::Redundant_Action; }

        m_insert_entity(std::to_underlying(e.type), e.rgba, e.id, e.description);

        using enum entity_type;
        switch (e.type)
        {
        case File:
            if (e.extra_info.size() < FILE_ENTITY_NUM_EXTRA_FIELDS ||
                (!std::holds_alternative<int64_t>(e.extra_info[0]) && 
                 !std::holds_alternative<int64_t>(e.extra_info[1]))
            )
            { return result_code::Invalid_Argument; }

            m_insert_file(e.id, e.extra_info[0], e.extra_info[1]);
            break;
        
        case Tag:
            if (e.extra_info.size() < TAG_ENTITY_NUM_EXTRA_FIELDS)
            { return result_code::Invalid_Argument; }

            if (!sz::is_valid_tag(e.id)) 
            { return result_code::Invalid_Argument; }

            m_insert_tag(e.id);
            break;
        
        case Alias:
            if (e.extra_info.size() < ALIAS_ENTITY_NUM_EXTRA_FIELDS ||
                (!std::holds_alternative<std::string>(e.extra_info[0]))
            )
            { return result_code::Invalid_Argument; }

            m_insert_alias(e.id, e.extra_info[0]);
            break;
        
        default:
            return result_code::Invalid_Argument;
        }
        return result_code::Success;
    }

    // TODO*: this function needs to use a manual transaction
    // does nothing if the entity does not exist
    void entity_database::delete_entity(const std::string& id)
    {
        using enum sqlite::status_code;
        using enum entity_type;

        if (!entity_exists(id))
        { return; }

        entity_type type = std::get<0>(m_entity_info(id));
        bool is_alias = false;
        std::string_view sql;
        
        switch(type)
        {
        case File:
            sql = "DELETE FROM file WHERE absolute_path = ?"; 
            break;

        case Tag:
            sql = "DELETE FROM tag WHERE full_name = ?";
            break;
        
        case Alias:
            sql = "DELETE FROM alias WHERE value = ?";
            is_alias = true;
            break;

        default: 
            throw exception(result_code::Unknown_Key); // got an unknown type from the database?
        }

        if (auto s = m_db.cmd(sql, id); s != Done)
        { throw exception(result_code::No_Action, m_db.error_message()); }

        if (!is_alias)
        {
            if (auto s = m_db.cmd("DELETE FROM link WHERE inset = ? OR outset = ?", id, id); s != Done)
            { throw exception(result_code::No_Action, m_db.error_message()); } // TODO: only compile this in debug builds
        }

        // entities backing aliases are deleted with trigger 'purge_entities_backing_aliases'
        // TODO: maybe we should adopt delete triggers for all child tables?
        if (is_alias) 
        { return; }

        if (auto s = m_db.cmd("DELETE FROM entity WHERE id = ?", id); s != Done)
        { throw exception(result_code::No_Action, m_db.error_message()); }
    }

    // returns a result_code and the first problematic element in the vector 'kvs' or 0 if there was no issue (use result_code to determine if index refers to a problematic index)
    // if you call this on an entity that doesn't exist, the library would remain unchanged
    // this function will not implicitly register new entities, and it does not check if an entity exists or not
    indexed_result_code entity_database::update_entity(const std::string& original_id, const std::vector<std::string>& kvs, bool old_id_as_alias)
    {
        static constexpr std::array<std::string_view, 3> s_updatable_columns{"id", "description", "rgba"};
        
        using enum result_code;
        indexed_result_code ret;

        std::string updated_id = original_id;
        std::vector<std::string> col_names;
        std::vector<sqlite::variant> vals;
        col_names.reserve(ENTITY_MUTABLE_COLUMN_COUNT);
        vals.reserve(ENTITY_MUTABLE_COLUMN_COUNT + 1);
        for (size_t i = 0; i < kvs.size(); ++i)
        {
            std::vector<std::string> kv = utility::split("=", kvs[i]);
            if (kv.size() != 2)
            {
                ret.code = Invalid_Argument;
                ret.index = i;
                return ret;
            }

            if (std::find(s_updatable_columns.begin(), s_updatable_columns.end(), kv[0]) != s_updatable_columns.end())
            {
                col_names.emplace_back(kv[0]);
                sqlite::variant x;
                if (kv[0] == "rgba")
                {
                    std::string_view s = kv[1];
                    bool parse_error = false;
                    if (s.starts_with("0x") || s.starts_with("0X") || s.starts_with("#"))
                    {
                        if (s.starts_with("#"))
                        { s = s.substr(1); }

                        if (result<int> v = utility::hstoi(s); v) 
                        { x = *v; }
                        else
                        { parse_error = true; }
                    }
                    else
                    {
                        if (result<int> v = utility::stoi(s); v)
                        { x = *v; }
                        else parse_error = true;
                    }
                    if (parse_error)
                    {
                        ret.code = Invalid_Argument;
                        ret.index = i;
                        return ret;
                    }
                }
                else if (kv[0] == "id")
                {
                    updated_id = kv[1];
                    x = kv[1];
                }
                else
                { x = kv[1]; }
                vals.emplace_back(std::move(x));
            }
            else
            {
                ret.code = Unknown_Key;
                ret.index = i;
                return ret;
            }
        }

        std::string col_names_expr = utility::join(" = ?,", col_names, true);
        auto sql = std::format("UPDATE entity SET {} WHERE id = ?", col_names_expr.substr(0, col_names_expr.size() - 1));
        vals.emplace_back(original_id);
        auto& q = m_db.query(sql, vals);
        THROW_IF_STEP_NOT_DONE(q);

        if (old_id_as_alias && (original_id != updated_id))
        {
            // return value of make_alias() doesn't matter here. it is a no_action if entity (original_id) doesn't exist
            // also, the desired alias id refers to the no longer used 'original_id'
            // if the entity we just updated was an alias, make_alias() returns false having done nothing to the library
            make_alias(updated_id, original_id);
        }
        ret.code = Success;
        return ret;
    }

    std::vector<std::string> entity_database::aliases(const std::string& entity_id)
    {
        auto sql = std::format(
            "SELECT value FROM alias,entity WHERE alias.target = ? AND entity.type = {} ORDER BY entity.create_time ASC", 
            ALIAS_ENTITY_TYPE
        );
        return m_db.query(sql, entity_id).xrows<std::string>();
    }
    
    std::string entity_database::alias_target(const std::string& alias)
    {
        // we expect one row here since value is a primary key
        std::string target_entity;

        if (auto& q = m_db.query("SELECT target FROM alias WHERE value = ?", alias); q.restep() == sqlite::status_code::Row)
        { target_entity = q.column<std::string>(); }

        return target_entity;
    }

    // also returns false if the id specifies an entity not in the library
    bool entity_database::is_alias(const std::string& id)
    {
        if (!entity_exists(id)) 
        { return false; }

        return m_entity_is_alias(id);
    }

    bool entity_database::is_alias_for(const std::string& alias, const std::string& entity_id)
    {
        auto& q = m_db.query("SELECT EXISTS(SELECT 1 FROM alias WHERE value = ? AND target = ?)", alias, entity_id);
        q.restep();
        return (q.column<int>() == 1);
    }

    // returns false if 'entity_id' does not refer to an entity in the database or if 'alias' refers to a different entity
    bool entity_database::make_alias(const std::string& entity_id, const std::string& alias)
    {
        if (!entity_exists(entity_id)) 
        { return false; }
        
        if (m_entity_is_alias(entity_id))
        { return false; }

        if (entity_exists(alias))
        {
            // is it an alias of entity_id?
            if (is_alias_for(alias, entity_id))
            { return true; } // nothing to do
            return false;
        }

        return register_entity(entity_database_info{
            .extra_info = {entity_id},
            .id = alias,
            .type = entity_type::Alias
        }) == result_code::Success;
    }

    // fromが存在しない場合問題になったりしない、リターン値がfalseになる
    // toが存在しない場合新しいツリーを作ることになる、すぐにfalseをもどす
    bool entity_database::link_makes_cycle(const std::string& from, const std::string& to)
    {
        if ((!entity_exists(from)) || (!entity_exists(to)))
        { return false; }
        return m_link_makes_cycle(from, to);
    }

    // when tagging files, the language is -from- file (the inset) -to- tag (the outset). 
    // returns false if one of the entities does not exist, if the link is ill-formed, or if making the link would produce a cycle in the tree
    bool entity_database::make_link(const std::string& from, const std::string& to)
    {
        if (!entity_exists(from) || !entity_exists(to))
        { return false; }
        
        if (!m_link_exists(from, to))
        {
            auto from_type = entity_type{std::get<0>(m_entity_info(from))};
            auto to_type = entity_type{std::get<0>(m_entity_info(to))};
            if (!m_is_well_formed_link(from_type, to_type))
            { return false; }
            if (m_link_makes_cycle(from, to))
            { return false; }
            m_make_link(from, to);
        }
        return true;
    }

    // use_aliases: allow aliases to appear in tags and have the function convert the alias to the actual tag in place before submitting a sql query
    // here's our boolean search string parse algorithm
    // possible forms of query_str: 'cat|dog'; 'cat,dog'; 'cat,mouse|dog', 'cat,~dog', 'cat|~dog'
    // from left to right: cat or dog, cat and dog, (cat and mouse) or dog, cat and NOT dog, cat or NOT dog
    // spaces are ignored unless they occur as part of a tag wrapped in ""
    // parenthesis not allowed
    // characters invalid for tags are our boolean operators: [ , meaning AND ] [ | meaning OR ] [ ~ meaning NOT ]
    // query strings look like this: cat,mouse,cheese_wheel|dog|hawk,bird
    // 2*2*2+1+3*3
    // we follow PE(M)D(A)S: M = AND, A = OR
    // template<typename T> T query_for_files(const std::string& query_str, bool use_aliases = false, bool recurse = true);
    sqlite::database_query entity_database::files(const entity_database_query& query)
    {
        static constexpr std::string_view and_op = " INTERSECT ";
        static constexpr std::string_view or_op = " UNION ";
        
        std::string sql;
        std::vector<sqlite::variant> binds;

        // for (const std::string& or_arg : utility::split("|", query_str))
        auto or_args = utility::split("|", query.string);
        for (size_t i = 0; i < or_args.size(); ++i)
        {
            auto or_arg = or_args[i];
            auto and_args = utility::split(",", or_arg);
            for (size_t j = 0; j < and_args.size(); ++j)
            {
                auto tag = std::string(utility::trim(and_args[j]));
                const bool negate = tag.starts_with("~");
                
                if (negate)
                {
                    if (tag.length() == 1) // the only character in the string was '~'
                    { tag.clear(); }
                    else
                    { tag = tag.substr(1); }
                }

                if (m_use_aliases && is_alias(tag))
                { tag = alias_target(tag); }

                using enum entity_type;
                if (m_recurse)
                {
                    auto [clause, extra_binds ] = m_recursive_inset_sql(tag, negate);
                    sql += clause + and_op.data();
                    binds.append_range(extra_binds);
                }
                else
                {
                    sql += m_inset_sql(File, negate) + and_op.data();
                    binds.emplace_back(tag);
                }
            }
            sql = sql.substr(0, sql.size() - and_op.size());
            sql += or_op.data();
        }
        sql = sql.substr(0, sql.size() - or_op.size());
        
        return m_db.make_query(sql, binds);
    }
    
    std::vector<std::string> entity_database::files(const entity_database_query& query, int n)
    {
        std::vector<std::string> ret;
        ret.reserve(n);
        auto q = files(query);
        
        for (int i = 0; ++q && (i < n); ++i)
        { ret.emplace_back(q.get<std::string>()); }
        
        return ret;
    }

    std::vector<std::string> entity_database::files(const std::filesystem::path& filename, int64_t modtime, uintmax_t size)
    {
        std::string sql = "SELECT absolute_path FROM file WHERE name LIKE ?";
        std::vector<sqlite::variant> binds = { std::format("%{}%", filename.string()) };
        if (modtime)
        {
            sql += " AND last_mod_time = ?";
            binds.emplace_back(modtime);
        }
        if (std::cmp_less(size, std::numeric_limits<uintmax_t>::max()))
        {
            sql += " AND size = ?";
            binds.emplace_back(size);
        }
        return m_db.query(sql, binds).xrows<std::string>();
    }

    // returns the amount of new links made to the file (amount of new (for the file) tags applied)
    // if the value returned doesn't match the size of the given vector, either tags in the vector already exist for the given file, or the given value contains invalid characters for a tag
    // the burden lies with the caller to ensure that all given tags are useable and to identify which tags might have been omitted because of invalid input
    // if the file doesn't already exist in-db, it will be registered in the database
    // if the file doesn't actually exist on the filesystem, file stat information will not be recorded (and the default value of 0 will be used)
    // if the file already exists in the db but the provided description and rgba values differ, the in-database value will not be changed. use update_entity() for this
    int entity_database::tag(const std::filesystem::path& file_path, const std::vector<std::string>& tags, std::string_view description_for_file, int rgba, bool dont_use_aliases, bool skip_nonexistent_tags)
    {
        int tags_applied = 0;
        const std::string file_path_str = file_path.string();
        
        using enum entity_type;
        entity_database_info e{
            .id = file_path_str,
            .description = std::string(description_for_file),
            .rgba = rgba,
            .type = File
        };

        if (auto stat = utility::stat(file_path); stat)
        { e.extra_info = {stat->modtime, static_cast<int64_t>(stat->size)}; }
        else
        { e.extra_info = {0, 0}; } // TODO: couldn't access file stat, we need to get it at the next available opportunity...

        register_entity(e);
        for (const std::string& tag : tags)
        {
            bool tag_is_alias = false;
            if (!entity_exists(tag)) 
            {
                if (skip_nonexistent_tags)
                { continue; }
                else if (
                    register_entity(entity_database_info{
                        .id = tag,
                        .type = Tag
                    }) == result_code::Invalid_Argument
                )
                { continue; } // the caller will have to compare the return value with the size of tags to determine if anything is amiss
            }
            else 
            {
                if (m_entity_is_alias(tag))
                {
                    if (dont_use_aliases)
                    { continue; }
                    tag_is_alias = true;
                }
            }

            std::string tag_to_use = (tag_is_alias ? alias_target(tag) : tag);
            if (!m_link_exists(file_path_str, tag_to_use))
            {
                m_make_link(file_path_str, tag_to_use);
                ++tags_applied;
            }
        }
        return tags_applied;
    }

    sqlite::database_query entity_database::tags(const entity_database_query& query)
    {
        bool file_query = std::filesystem::exists(query.string);
        if (file_query)
        {
            // copied from tags(filesytem::path)
            return m_db.make_query("SELECT outset FROM link WHERE inset = ?", query.string);
        }
        
        // tag_query
        auto q = files(query);
        // this works because files cannot appear in outset
        q.set_sql(std::format("SELECT outset FROM ({}) f,link WHERE f.inset = link.inset", q.sql()));
        q.rebind();
        return q;
        
    }

    std::vector<std::string> entity_database::tags(const std::filesystem::path& path)
    {
        return m_db.query("SELECT outset FROM link WHERE inset = ?", path.string()).xrows<std::string>();
    }

    std::unordered_map<std::string, int> entity_database::tags(const entity_database_query& query, int n)
    {
        std::unordered_map<std::string, int> t;
        auto q = files(query);
        int i = 0;
        while (++q && (i < n))
        {
            std::vector<std::string> tags_for_file = tags(q.get<std::filesystem::path>());
            for (auto& tag : tags_for_file)
            {
                if (t.contains(tag))
                { t[tag] += 1; }
                else
                { t.emplace(tag, 1); }
            }
            ++i;
        }
        return t;
    }

    /* private */

    void entity_database::m_init()
    {
        /* ---- DEFINITIONS OF SQLITE TABLES USED BY THE CLASS ARE FOUND HERE ---- */
        // * a file, a tag, and an alias are entities
        // * the files table helps us track files on the filesystem
        // * a tag table helps us understand tags in the context of other tags (namespaced tags, distinguish from aliases, etc)

        // * the link table describes relation between entities
        //   'outset' is a term used to describe the set of entities that describe another entity
        //   'inset' is a term used to describe the set of entities that are described by other entities
        // any value in these two columns refers to the id of an entity

        // this is likely a nightmare to maintain. if any table were to have its definition altered, hunting down all of the places
        // that also need to reflect the changes is more trouble than it needs to be. if only C++ had reflection support...
        static constexpr std::array<std::string_view, 9> s_init_cmds = {
            "PRAGMA journal_mode = wal",
            "PRAGMA foreign_keys = ON",

            R"sql(
            CREATE TABLE IF NOT EXISTS entity (
                type INT NOT NULL, 
                create_time INT NOT NULL DEFAULT (unixepoch()), 
                mod_time INT NOT NULL DEFAULT (unixepoch()), 
                rgba INT NOT NULL DEFAULT (255),
                id TEXT NOT NULL PRIMARY KEY ON CONFLICT ROLLBACK COLLATE RTRIM,
                description TEXT
            ) STRICT, WITHOUT ROWID
            )sql",

            // https://stackoverflow.com/questions/21388820/how-to-get-the-last-index-of-a-substring-in-sqlite
            R"sql(
            CREATE TABLE IF NOT EXISTS file (
                absolute_path TEXT NOT NULL PRIMARY KEY ON CONFLICT ROLLBACK, 
                last_mod_time INT,
                size INT,

                parent TEXT NOT NULL GENERATED ALWAYS AS (
                    CASE WHEN rtrim(absolute_path, '/') = '' THEN '/'
                    ELSE rtrim( rtrim(absolute_path, '/'), replace(rtrim(absolute_path, '/'), '/', '') ) 
                    END
                ) VIRTUAL,

                name TEXT NOT NULL GENERATED ALWAYS AS (
                    CASE WHEN rtrim(absolute_path, '/') = '' THEN ''
                    ELSE replace(rtrim(absolute_path, '/'), rtrim(rtrim(absolute_path, '/'), replace(rtrim(absolute_path, '/'), '/', '')), '') 
                    END
                ) VIRTUAL,

                CHECK(replace(rtrim(absolute_path, '/'), rtrim( rtrim(absolute_path, '/'), replace(rtrim(absolute_path, '/'), '/', '') ), '') != ''), 
                FOREIGN KEY (absolute_path) REFERENCES entity(id) ON DELETE RESTRICT ON UPDATE CASCADE
            ) STRICT, WITHOUT ROWID
            )sql",
            
            R"sql(
            CREATE TABLE IF NOT EXISTS tag (
                full_name TEXT NOT NULL PRIMARY KEY ON CONFLICT ROLLBACK, 
                name TEXT GENERATED ALWAYS AS (substr( full_name, instr(full_name, ':') + 1 ) ) VIRTUAL, 
                namespace TEXT GENERATED ALWAYS AS (
                    CASE WHEN instr(full_name, ':') = 0 THEN '' 
                    ELSE substr( full_name, 1, length(full_name) - instr(full_name, ':') ) 
                    END 
                ) VIRTUAL, 
                FOREIGN KEY (full_name) REFERENCES entity(id) ON DELETE RESTRICT ON UPDATE CASCADE
            ) STRICT, WITHOUT ROWID
            )sql",

            R"sql(
            CREATE TABLE IF NOT EXISTS link (
                outset TEXT NOT NULL REFERENCES entity(id) ON DELETE RESTRICT ON UPDATE CASCADE, 
                inset TEXT NOT NULL REFERENCES entity(id) ON DELETE RESTRICT ON UPDATE CASCADE, 
                PRIMARY KEY(outset,inset) ON CONFLICT ROLLBACK
            ) STRICT, WITHOUT ROWID
            )sql",

            // difference with the link table is that we dont do any recursion on the alias table
            // target references tag(full_name) to ensure target is always a valid tag
            R"sql(
            CREATE TABLE IF NOT EXISTS alias (
                value TEXT NOT NULL REFERENCES entity(id) ON DELETE RESTRICT ON UPDATE CASCADE,
                target TEXT NOT NULL REFERENCES tag(full_name) ON DELETE CASCADE ON UPDATE CASCADE,
                PRIMARY KEY(value, target) ON CONFLICT ROLLBACK
            ) STRICT, WITHOUT ROWID
            )sql",

            R"sql(
            CREATE TRIGGER IF NOT EXISTS update_entity_mod_time
            AFTER UPDATE ON entity
            FOR EACH ROW WHEN OLD.mod_time = NEW.mod_time
            BEGIN
                UPDATE entity SET mod_time = (unixepoch())
                WHERE id = NEW.id;
            END
            )sql",

            // this trigger will run if aliases are implicitly deleted when the target they point to is deleted via 'ON DELETE CASCADE'
            // library::delete_entity() will skip deleting from the entity table for aliases because of this (in that function, rows from the alias table are deleted first)
            // might be worth just having delete triggers for all of the child tables
            R"sql(
            CREATE TRIGGER IF NOT EXISTS purge_entities_backing_aliases
            AFTER DELETE ON alias
            BEGIN
                DELETE FROM entity WHERE entity.id = OLD.value;
            END
            )sql"
        };
        
        using enum sqlite::status_code;
        for (std::string_view cmd : s_init_cmds) 
        { 
            if (m_db.cmd(cmd) != Done)
            { throw exception(result_code::Init_Problem, m_db.error_message()); }
        }
    }

    // allowed: from file to tag and from tag to tag
    inline bool entity_database::m_is_well_formed_link(entity_type from_type, entity_type to_type)
    {
        using enum entity_type;
        if (from_type == Alias || to_type == File)
        { return false; }
        else if (to_type == File)
        { return false; }

        return true;
    }

    inline void entity_database::m_insert_entity(int type, int rgba, const std::string &id, const std::string &description)
    {
        // * if a constraint error or SQLITE_MISUSE occurs here, there is a problem with our logic
        //   this method should only be called after we have ensured the entity doesn't already exist.
        // * if this method is called with the same entity twice in a row, we will get a cached query already in the 'done' state on the second invocation.
        //   calling step() on the query in this state will return 'misuse' - we shouldnt be calling the same method with the same input twice
        auto& q = m_db.query("INSERT INTO entity(id,type,rgba,description) VALUES(?,?,?,?)", id, type, rgba, description);
        THROW_IF_STEP_NOT_DONE(q);
    }

    inline void entity_database::m_insert_file(const std::string &path, int64_t modtime, uintmax_t size)
    {
        auto& q = m_db.query(
            "INSERT INTO file(absolute_path, last_mod_time, size) VALUES(?,?,?)", 
            path, modtime, size);
        THROW_IF_STEP_NOT_DONE(q);
    }

    inline void entity_database::m_insert_tag(const std::string &fully_qualified_tag)
    {
        auto& q = m_db.query("INSERT INTO tag(full_name) VALUES(?)", fully_qualified_tag);
        THROW_IF_STEP_NOT_DONE(q);
    }

    inline void entity_database::m_insert_alias(const std::string &target_entity, const std::string &new_alias)
    {
        auto& q = m_db.query("INSERT INTO alias(value, target) VALUES(?, ?)", new_alias, target_entity);
        THROW_IF_STEP_NOT_DONE(q);
    }

    inline std::vector<sqlite::variant> entity_database::m_file_info(const std::string &id)
    {
        auto& q = m_db.query("SELECT absolute_path, last_mod_time, size FROM file WHERE absolute_path = ?", id);
        q.restep();
        return q.row();
    }

    inline bool entity_database::m_link_exists(const std::string& from, const std::string& to)
    {
        auto& q = m_db.query("SELECT EXISTS(SELECT 1 FROM link WHERE inset = ? AND outset = ?)", from, to);
        q.restep();
        return q.column<int>() == 1;
    }

    inline std::string entity_database::m_inset_sql(entity_type type, bool negate)
    {
        std::string common_expr = std::format(
            "SELECT inset FROM link,entity WHERE entity.id = link.inset AND entity.type = {}", std::to_underlying(type)
        );
        std::string suffix_expr = (negate ? 
            std::format("EXCEPT {} AND link.outset = ?", common_expr) : 
            "AND link.outset = ?"
        );

        return std::format("{} {}", common_expr, suffix_expr);
    }

    inline std::vector<std::string> entity_database::m_inset(const std::string& outset_id, entity_type type, bool negate)
    {
        return m_db.query(m_inset_sql(type, negate), outset_id).xrows<std::string>();
    }

    inline std::tuple<std::string, std::vector<sqlite::variant>> entity_database::m_recursive_inset_sql(const std::string& outset_id, bool negate)
    {
        using enum entity_type;
        std::string sql = m_inset_sql(File, negate) + " INTERSECT ";
        std::vector<sqlite::variant> binds;
        binds.emplace_back(outset_id);
        m_for_each_tag(outset_id, [&](const std::string& tag) -> bool
        {
            // if we return true here, the database has a cycle in the link tree
            if (tag == outset_id)
            { return true; }

            sql += m_inset_sql(File, negate) + " INTERSECT ";
            binds.emplace_back(tag);
            return false;
        });
        sql = sql.substr(0, sql.size() - 11); // remove last " INTERSECT " from string
        return { sql, binds };
    }

    inline std::unordered_set<std::string> entity_database::m_recursive_inset(const std::string& outset_id, bool negate)
    {
        using enum entity_type;
        std::unordered_set<std::string> f;
        f.insert_range(m_inset(outset_id, File, negate));
        m_for_each_tag(outset_id, [&](const std::string& tag) -> bool
        {
            // if we return true here, the database has a cycle in the link tree
            if (tag == outset_id)
            { return true; }

            f.insert_range(m_inset(tag, File, negate));
            return false;
        });
        return f;
    }

    // more thorough checks are the responsibility of the caller
    // we only check for tag entities - links from an inset entity of type tag to an outset entity of type file are not valid
    // check if the value we are trying to set as the inset does not already occur in the link tree as an outset
    // check if value we are trying to use as inset doesn't occur as an outset that leads back to the value we are trying to use as the new outset
    inline bool entity_database::m_link_makes_cycle(const std::string& inset_id, const std::string& outset_id)
    {
        return m_for_each_tag(inset_id, [&](const std::string& tag) -> bool { return outset_id == tag; });
    }

    inline void entity_database::m_make_link(const std::string &from, const std::string &to)
    {
        auto& q = m_db.query("INSERT OR IGNORE INTO link(outset, inset) VALUES(?,?)", to, from);
        THROW_IF_STEP_NOT_DONE(q);
    }

    // if the entity doesn't exist, default constructed values are returned in the tuple
    inline std::tuple<entity_type, uintmax_t, uintmax_t, int, std::string, std::string> entity_database::m_entity_info(const std::string& id)
    {
        using enum sqlite::status_code;
        auto& q = m_db.query("SELECT type,create_time,mod_time,rgba,id,description FROM entity WHERE id = ?", id);
        q.restep();
        return q.row<entity_type, uintmax_t, uintmax_t, int, std::string, std::string>();
    }

    inline bool entity_database::m_entity_is_alias(const std::string &id)
    {
        return std::get<0>(m_entity_info(id)) == entity_type::Alias;
    }
}
