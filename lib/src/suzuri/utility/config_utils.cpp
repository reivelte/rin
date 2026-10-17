// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <iostream>
#include <cstdlib>
#include <sstream>
#include "config_utils.hpp"

namespace sz
{
    SZ_API std::filesystem::path get_home_directory()
    {
        std::filesystem::path p;
        #ifdef _WIN32
        const char* home = std::getenv("USERPROFILE");
        #else
        const char* home = std::getenv("HOME");
        #endif

        if (home)
        { p = home; }
        return p;
    }

    SZ_API std::filesystem::path get_xdg_directory(data_directory_type type)
    {
        using enum data_directory_type;
#ifdef _WIN32
        std::filesystem::path user = get_home_directory();
        return (user / "AppData" / "Local").string();
#else
        const char* p = nullptr;
        switch (type)
        {
            case Config: { p = std::getenv("$XDG_CONFIG_HOME"); break; }
            case State:  { p = std::getenv("$XDG_STATE_HOME"); break; }
            case Data:   { p = std::getenv("$XDG_DATA_HOME"); break; }
            case Cache:  { p = std::getenv("$XDG_CACHE_HOME"); break; }
            default: break;
        }

        if (p)
        { return p; }

        const std::string home = get_home_directory().string();
        switch (type)
        {
            case Config: { return home + "/.config"; }
            case State:  { return home + "/.local/state"; }
            case Data:   { return home + "/.local/share"; }
            case Cache:  { return home + "/.cache"; }
            default: break;
        }
        return {};
#endif
    }

    std::filesystem::path get_config_path(config_type type)
    {
        using enum config_type;
        switch (type)
        {
        default:
        case Static: { return get_xdg_directory(data_directory_type::Config) / "rin" / "config.toml"; }
        case State: { return get_xdg_directory(data_directory_type::State) / "rin" / "state.toml"; }
        }
        return {};
    }

    // TODO: verify some metadata with the database/config files themselves
    bool is_valid_domain(const std::filesystem::path& path)
    {
        auto name = path.stem();
        auto pname = path.parent_path().stem();
        return std::filesystem::exists(path) && name == pname;
    }

    sz::result<toml_config> toml_config::create_or_open(const std::filesystem::path& p, config_type t)
    {
        toml_config config;
        auto ret = config.set_config(p, t);
        
        if (!ret)
        { return ret.error(); }

        return config;
    }

    toml_config::toml_config(const std::filesystem::path& p, config_type t) : m_filepath(p), m_type(t)
    {
        m_init();
    }

    sz::result<void> toml_config::set_config(const std::filesystem::path& p, config_type t)
    {
        m_filepath = p;
        m_type = t;
        auto data = m_init();
        
        if (!data)
        { return data.error(); }

        return {};
    }

    bool toml_config::has_value(std::string key) const
    {
        auto data = m_read();
        if (data)
        {
            auto keys = sz::utility::split(".", key);
            return m_keys_exist(keys, *data);
        }
        return false;
    }

    bool toml_config::available() const
    {
        if (!std::filesystem::exists(m_filepath))
        { return false; }
        
        if (auto data = m_read(); !data)
        { return false; }

        return true;
    }

    sz::result<toml::ordered_value> toml_config::m_init()
    {
        if (!std::filesystem::exists(m_filepath))
        {
            std::ofstream out(m_filepath);
            out <<
                R"toml(

                )toml"
            << std::endl;
        }
        return m_read();
    }

    toml::ordered_value& toml_config::m_data_for_keys(const std::vector<std::string>& keys, toml::ordered_value& data) const
    {
        if (keys.size() == 1)
        {
            return data.at(keys[0]);
        }
        
        return data.at(keys[0]).at(keys[1]);
    }

    bool toml_config::m_keys_exist(const std::vector<std::string>& keys, toml::ordered_value& data) const
    {
        if (keys.size() == 1)
        {
            return data.contains(keys[0]);
        }
        return data.contains(keys[0]) && data.at(keys[0]).contains(keys[1]);
    }

    sz::result<toml::ordered_value> toml_config::m_read() const
    {
        auto data = toml::try_parse<toml::ordered_type_config>(m_filepath);
        if (data.is_err())
        {
            auto err = data.unwrap_err();
            std::stringstream ss;
            for (const auto& e : err)
            { ss << e << '\n'; }
            return sz::common_error(sz::result_code::Incomplete_Result, ss.str());
        }
        return data.unwrap();
    }

} // namespace sz
