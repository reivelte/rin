// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <string_view>
#include <unordered_set>
#include <unordered_map>
#include <concepts>
#include <type_traits>
#include <queue>
#include <sz_export.hpp>
#include "../sqlite/database.hpp"
#include "../exception.hpp"
#include "../sqlite/query.hpp"
#include "common.hpp"

namespace sz
{
    struct entity_database_info
    {
        std::vector<sqlite::variant> extra_info{};
        std::string id{};
        std::string description{};
        uintmax_t createtime{0};
        uintmax_t modtime{0};
        int rgba{255};
        entity_type type{entity_type::Invalid};

        explicit consteval operator int() const noexcept { return static_cast<int>(std::to_underlying(type)); }
        constexpr sqlite::variant operator[](size_t idx) const
        {
            switch (idx)
            {
            case 0: return std::to_underlying(type);
            case 1: return rgba;
            case 2: return id;
            case 3: return description;
            default:
                break;
            }
            throw exception(result_code::Index_Out_Of_Bounds);
        }
        constexpr std::string_view field_name(size_t idx) const
        {
            switch(idx)
            {
                case 0: return "type";
                case 1: return "rgba";
                case 2: return "id";
                case 3: return "description";
            }
            throw exception(result_code::Index_Out_Of_Bounds);
        }
        consteval size_t size() const noexcept { return static_cast<size_t>(ENTITY_TOTAL_COLUMN_COUNT); }
    };

    struct entity_database_query
    {
        std::string string;

        inline operator const std::string&() const noexcept { return string; }
    };

    class SZ_API entity_database
    {
        public:
        entity_database();
        explicit entity_database(const std::filesystem::path& database_path, bool read_only = false);
        entity_database(entity_database&& rhs) :m_db(std::move(rhs.m_db)) {}
        entity_database(const entity_database&) = delete;
        ~entity_database() = default;

        explicit inline                 operator bool() const { return m_db.status() == sqlite::status_code::OK; }
        inline entity_database&         operator=(entity_database&& rhs) { m_db = std::move(rhs.m_db); return *this; }
        entity_database&                operator=(const entity_database&) = delete;
        
        void set_database(const std::filesystem::path& path);
        inline void clear_cache() noexcept { m_db.clear_cache(); }
        
        inline const sqlite::database&  database() const noexcept { return m_db; }

        /* entities */
        bool entity_exists(const std::string& id);
        entity_database_info entity_info(const std::string& id);
        result_code register_entity(const entity_database_info& e);
        void delete_entity(const std::string& id);
        indexed_result_code update_entity(const std::string& original_id, const std::vector<std::string>& kvs, bool old_id_as_alias = true);

        /* aliases */
        std::vector<std::string> aliases(const std::string& entity_id);
        std::string alias_target(const std::string& alias);
        bool is_alias(const std::string& id);
        bool is_alias_for(const std::string& alias, const std::string& entity_id);
        bool make_alias(const std::string& entity_id, const std::string& alias);

        /* relating entities */
        bool link_makes_cycle(const std::string& from, const std::string& to);
        bool make_link(const std::string& from, const std::string& to);

        /* getting files */        
        sqlite::database_query files(const entity_database_query& query);
        std::vector<std::string> files(const entity_database_query& query, int n);
        std::vector<std::string> files(const std::filesystem::path& filename, int64_t modtime = 0, uintmax_t size = std::numeric_limits<uintmax_t>::max());
        
        /* get and set tags for an entity or entities */
        int tag(const std::filesystem::path& file_path, const std::vector<std::string>& tags, std::string_view description_for_file = "", int rgba = 255, bool dont_use_aliases = false, bool skip_nonexistent_tags = false);
        sqlite::database_query tags(const entity_database_query& query);
        std::vector<std::string> tags(const std::filesystem::path& path);
        std::unordered_map<std::string, int> tags(const entity_database_query& query, int n);

        private:
        template <typename Func> requires (std::invocable<Func, std::string> && std::same_as<std::invoke_result_t<Func, std::string>, bool>)
        inline bool m_for_each_tag(const std::string& outset_id, Func func);

        void m_init();
        inline bool m_is_well_formed_link(entity_type from_type, entity_type to_type);
        inline void m_insert_entity(int type, int rgba, const std::string& id, const std::string& description);
        inline void m_insert_file(const std::string& path, int64_t modtime, uintmax_t size);
        inline void m_insert_tag(const std::string& fully_qualified_tag);
        inline void m_insert_alias(const std::string& target_entity, const std::string& new_alias);
        inline std::vector<sqlite::variant> m_file_info(const std::string& id);
        inline bool m_link_exists(const std::string& from, const std::string& to);
        inline std::string m_inset_sql(entity_type type, bool negate);
        inline std::vector<std::string> m_inset(const std::string& outset_id, entity_type type, bool negate);
        inline std::tuple<std::string, std::vector<sqlite::variant>> m_recursive_inset_sql(const std::string& outset_id, bool negate);
        inline std::unordered_set<std::string> m_recursive_inset(const std::string& outset_id, bool negate);
        inline bool m_link_makes_cycle(const std::string& from, const std::string& to); // unchecked version of link_makes_cycle()
        inline void m_make_link(const std::string& from, const std::string& to); // unchecked version of make_link()
        inline std::tuple<entity_type, uintmax_t, uintmax_t, int, std::string, std::string> m_entity_info(const std::string& id);
        inline bool m_entity_is_alias(const std::string& id);

        private:
        sqlite::database m_db;
        bool m_read_only;
        bool m_use_aliases, m_recurse;
    };

    /* template definitions */
    
    template <typename Func> requires (std::invocable<Func, std::string> && std::same_as<std::invoke_result_t<Func, std::string>, bool>)
    inline bool entity_database::m_for_each_tag(const std::string& outset_id, Func func)
    {
        using enum entity_type;
        std::queue<sqlite::database_query> x;
        for (x.push(m_db.make_query(m_inset_sql(Tag, false), outset_id)); x.size(); x.pop())
        {
            for (const std::string tag : x.front())
            {
                if (func(tag))
                { return true; }
                x.push(m_db.make_query(m_inset_sql(Tag, false), tag));
            }
        }
        return false;
    }

} // namespace sz