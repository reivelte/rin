// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <mutex>
#include <filesystem>
#include <sz_export.hpp>
#include "../utility/config_utils.hpp"
#include "../thread/guarded_ptr.hpp"

namespace sz
{
    class SZ_API core_ctl
    {
        public:
        static void init(int argc, char** argv);
        static void exit();
        static core_ctl* get();
        
        public:
        core_ctl(const core_ctl&) = delete;
        void operator=(const core_ctl&) = delete;

        void set_config(const std::filesystem::path& path, sz::config_type type);

        toml_config* config(sz::config_type type);

        private:
        core_ctl() {};

        private:
        std::mutex m_mutex;
        toml_config m_main_config;
        toml_config m_state_config;

    };

    #define SZ_MAIN_CONF sz::core_ctl::get()->config(sz::config_type::Static)
    #define SZ_STATE_CONF sz::core_ctl::get()->config(sz::config_type::State)
    
} // namespace sz
