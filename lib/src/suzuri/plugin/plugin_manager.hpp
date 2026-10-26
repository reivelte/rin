// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <mutex>
#include <set>
#include <unordered_map>
#include <filesystem>
#include <string>
#include <string_view>
#include <tuple>
#include <sz_export.hpp>
#include "../sqlite/database.hpp"
#include "plugin.hpp"

namespace sz
{
    class SZ_API plugin_manager
    {
        public:
        struct search_result
        {
            std::filesystem::path path;
            int index;

            search_result(const std::tuple<std::filesystem::path, int>& data) :
                path(std::get<0>(data)), index(std::get<1>(data)) {}
        };

        static void init();
        static void exit();
        static plugin_manager* get();

        plugin_manager(const plugin_manager&) = delete;
        plugin_manager& operator=(const plugin_manager&) = delete;

        result<plugin_descriptor> descriptor(std::filesystem::path path, int plugin_index) const;
        std::vector<search_result> find(std::string_view type, std::string_view features = "") const;
        result<native_plugin*> plugin(std::filesystem::path path, int plugin_index) const;

        private:
        plugin_manager();
        result<void> m_init_database();
        result<void> m_read_plugin_data();
        result<void> m_init();
        bool m_plugin_exists(std::filesystem::path path, int plugin_index) const;

        private:
        mutable std::mutex m_mutex;
        mutable sqlite::database m_db; // most of the class functions are non-const due to caching
        std::unordered_map<std::filesystem::path, shared_library> m_modules;
    };
    
} // namespace sz
