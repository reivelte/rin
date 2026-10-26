// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <iostream>
#include <ranges>
#include "../utility/config_utils.hpp"
#include "../utility/string.hpp"
#include "plugin_manager.hpp"

namespace sz
{
    static plugin_manager* s_instance = nullptr;

    void plugin_manager::init()
    {
        if (s_instance)
        { return; }

        s_instance = new plugin_manager;
    }
    
    void plugin_manager::exit()
    {
        if (s_instance)
        { delete s_instance; }

        s_instance = nullptr;
    }
    
    plugin_manager* plugin_manager::get()
    {
        return s_instance;
    }

    result<plugin_descriptor> plugin_manager::descriptor(std::filesystem::path path, int plugin_index) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_plugin_exists(path, plugin_index))
        {
            auto&q = m_db.query(
                "SELECT name,type,version,features FROM plugin WHERE module_path = ? AND plugin_index = ?",
                path, plugin_index
            );
            auto [name, type, ver, features] = (++q).row<std::string, std::string, std::string, std::string>();
            std::vector<std::string> s = utility::split(features, ";");
            return plugin_descriptor{
                .name = name,
                .type = type,
                .version = ver,
                .features = std::unordered_set<std::string>(s.begin(), s.end())
            };
        }
        return common_error(result_code::Index_Out_Of_Bounds);
    }

    // caller takes ownership of returned pointer
    result<native_plugin*> plugin_manager::plugin(std::filesystem::path path, int plugin_index) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_plugin_exists(path, plugin_index))
        {
            return native_plugin::from_shared_library(m_modules.at(path), plugin_index);
        }
        return common_error(result_code::Index_Out_Of_Bounds);
    }

    std::vector<plugin_manager::search_result> plugin_manager::find(std::string_view type, std::string_view features) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::vector<search_result> result;
        std::vector<std::string> features_arr = utility::split(";", features);
        std::vector<sqlite::variant> binds;
        std::string sql = "SELECT plugin.module_path,plugin.plugin_index FROM plugin,feature WHERE plugin.type = ?";
        binds.emplace_back(type);
        
        if (features.size())
        {
            for (std::string_view s : features_arr)
            {
                auto kv = utility::split(":", s);
                if (kv.size())
                {
                    sql += " AND";
                    binds.emplace_back(kv[0]);
                    if (kv.size() == 2)
                    {
                        sql += " (feature.key = ?";
                        for (const std::string& v : utility::split(",", kv[1]))
                        {
                            sql += " AND feature.value LIKE ?";
                            binds.emplace_back("%" + v + "%");
                        }
                        sql += " AND plugin.plugin_index = feature.plugin_index)";
                    }
                    else
                    { sql += " feature.key = ?"; }
                }
            }
        }
        sqlite::database_query& q = m_db.query(sql, binds);
        while (++q)
        { result.emplace_back(q.row<std::filesystem::path, int>()); }
        
        return result;
    }

    plugin_manager::plugin_manager() :
        m_db(":memory:")
    {
        if (auto init_result = m_init(); !init_result)
        { sz::check_error(init_result); }
    }

    result<void> plugin_manager::m_init_database()
    {
        static constexpr std::array<std::string_view, 4> s_init_cmds = {
            "PRAGMA journal_mode = wal",
            "PRAGMA foreign_keys = ON",
            R"sql(
            CREATE TABLE IF NOT EXISTS plugin (
                module_path  TEXT NOT NULL,
                name         TEXT NOT NULL,
                type         TEXT NOT NULL,
                version      TEXT NOT NULL,
                plugin_index INT NOT NULL,
                PRIMARY KEY(module_path, plugin_index)
            ) STRICT, WITHOUT ROWID
            )sql",

            R"sql(
            CREATE TABLE IF NOT EXISTS feature (
                module_path     TEXT NOT NULL,
                plugin_index    INT NOT NULL,
                key             TEXT NOT NULL,
                value           TEXT,
                PRIMARY KEY(module_path, plugin_index, key)
            ) STRICT, WITHOUT ROWID
            )sql"
        };

        for (const std::string_view cmd : s_init_cmds)
        {
            if (m_db.cmd(cmd) != sqlite::status_code::Done)
            { return common_error(result_code::Init_Problem, m_db.error_message(std::string(cmd))); }
        }
        return {};
    }

    result<void> plugin_manager::m_read_plugin_data()
    {
        namespace fs = std::filesystem;
        
        fs::path plugin_dir = get_xdg_directory(data_directory_type::Data) / "suzuri" / "plugins";
        std::error_code ec;

        if (!fs::exists(plugin_dir, ec))
        {
            if (!ec)
            { fs::create_directories(plugin_dir, ec); }
        }
        
        for (const fs::directory_entry& entry : fs::recursive_directory_iterator(plugin_dir, fs::directory_options::skip_permission_denied, ec))
        {
            if (auto path = entry.path(); path.extension() == ".szplugin")
            {
                if (shared_library lib(entry.path()); lib)
                {
                    if (auto symbol = lib.load_symbol("list"); symbol)
                    {
                        std::vector<plugin_descriptor> (*fn)(void);
                        *(void**)(&fn) = *symbol;
                        auto plugins = (*fn)();
                        for (int i = 0; std::cmp_less(i, plugins.size()); ++i)
                        {
                            const plugin_descriptor& d = plugins[i];
                            m_db.cmd(
                                "INSERT INTO plugin(module_path,name,type,version,plugin_index) VALUES(?,?,?,?,?)",
                                path, d.name, d.type, d.version, i
                            );
                            for (std::string_view feature_str : d.features)
                            {
                                auto key = feature_str.substr(0, feature_str.find_first_of(":"));
                                auto value = (key.size() + 1 < feature_str.size()) ? feature_str.substr(key.size() + 1) : "";
                                m_db.cmd(
                                    "INSERT INTO feature(module_path,plugin_index,key,value) VALUES(?,?,?,?)",
                                    path, i, key, value
                                );
                            }
                        }
                        m_modules.emplace(path, lib);
                    }
                    else
                    { sz::check_error(symbol); }
                }
            }
        }
        return ec ? common_error(ec) : result<void>();
    }

    result<void> plugin_manager::m_init()
    {
        if (auto init_result = m_init_database(); !init_result)
        { return init_result.error(); }

        return m_read_plugin_data();
    }

    bool plugin_manager::m_plugin_exists(std::filesystem::path path, int plugin_index) const
    {
        sqlite::database_query& q = m_db.query(
            "SELECT EXISTS(SELECT 1 FROM plugin WHERE module_path = ? AND plugin_index = ?)",
            path, plugin_index
        );
        return (++q).column<int>() == 1;
    }

} // namespace sz
