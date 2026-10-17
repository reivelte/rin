// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ctl.hpp"

namespace sz
{
    static core_ctl* s_instance = nullptr;

    void core_ctl::init(int argc, char** argv)
    {
        if (s_instance)
        { return; }

        (void)argc;
        (void)argv;
        s_instance = new core_ctl;

        auto ensure_directory_exists = [](const std::filesystem::path& path) -> bool
        {
            if (std::error_code ec; !std::filesystem::exists(path))
            {
                if (ec)
                { return false; }
                return std::filesystem::create_directories(path, ec);
            }
            return true;
        };

        using enum config_type;
        auto main_config_path = get_config_path(Static);
        auto state_config_path = get_config_path(State);

        if (ensure_directory_exists(main_config_path.parent_path()))
        { s_instance->set_config(main_config_path, Static); }

        if (ensure_directory_exists(state_config_path.parent_path()))
        { s_instance->set_config(state_config_path, State); }

    }

    void core_ctl::exit()
    {
        if (s_instance)
        { delete s_instance; }

        s_instance = nullptr;
    }

    core_ctl* core_ctl::get()
    {
        return s_instance;
    }

    void core_ctl::set_config(const std::filesystem::path& path, sz::config_type type)
    {
        using enum sz::config_type;
        
        const std::scoped_lock<std::mutex> lock(m_mutex);
        auto config = toml_config::create_or_open(path, type);

        if (config)
        {
            switch (type)
            {
            default:
            case Static: { m_main_config = *config; break; }
            case State: { m_state_config = *config; break; }
            }
        }
        // TODO: log warning if create_or_open() returned an error
    }

    toml_config* core_ctl::config(sz::config_type type)
    {
        using enum sz::config_type;
        switch (type)
        {
        default:
        case Static: { return &m_main_config; }
        case State: { return &m_state_config; }
        }
        return &m_main_config;
    }

} // namespace sz
