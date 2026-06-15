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

    SZ_API std::string get_xdg_directory(data_directory_type type)
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
            case State: { p = std::getenv("$XDG_STATE_HOME"); break; }
            case Data: { p = std::getenv("$XDG_DATA_HOME"); break; }
            default: break;
        }

        if (p)
        { return p; }

        const std::string home = std::getenv("HOME");
        switch (type)
        {
            case Config: { return home + "/.config"; }
            case State: { return home + "/.local/state"; }
            case Data: { return home + "/.local/share"; }
            default: break;
        }
        return {};
#endif
    }

    // TODO: verify some metadata with the database/config files themselves
    SZ_API bool is_valid_domain(const std::filesystem::path& path)
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

    toml_config::toml_config(const std::filesystem::path &p, config_type t) : m_filepath(p), m_type(t)
    {
        m_init();
    }

    bool toml_config::has_value(std::string key)
    {
        auto data = m_read();
        if (data)
        {
            auto keys = sz::utility::split(".", key);
            return m_keys_exist(keys, *data);
        }
        return false;
    }

    sz::result<void> toml_config::set_config(const std::filesystem::path &p, config_type t)
    {
        m_filepath = p;
        m_type = t;
        auto data = m_init();
        
        if (!data)
        { return data.error(); }

        return {};
    }

    void toml_config::set_window_size(int w, int h)
    {
        auto data = toml::parse<toml::ordered_type_config>(m_filepath);
        
        if (!(data.contains("window") && data.at("window").contains("size")))
        {
            data["window"]["size"] = toml::array{};
        }

        auto& s = data.at("window").at("size").as_array();
        s.clear();
        s.push_back(w);
        s.push_back(h);
        
        m_write(data);
    }

    void toml_config::set_recently_used_domain(const std::filesystem::path& path)
    {
        auto data = toml::parse<toml::ordered_type_config>(m_filepath);
        data["domain"]["recently_used"] = path.string();
        m_write(data);
    }

    void toml_config::add_domain_location(const std::filesystem::path& path)
    {
        auto data = toml::parse<toml::ordered_type_config>(m_filepath);

        if (!(data.contains("domain") && data.at("domain").contains("locations")))
        {
            data["domain"]["locations"] = toml::array{};
        }
        auto& v = data.at("domain").at("locations");
        v.push_back(path.string());

        m_write(data);
    }

    vector2 toml_config::window_size() const
    {
        auto data = toml::parse(m_filepath);
        if (data.contains("window") && data.at("window").contains("size"))
        {
            auto [width, height] = toml::find<std::tuple<int, int>>(data, "window", "size");
            return vector2{
                .x = width,
                .y = height
            };
        }
        return {};
    }

    std::filesystem::path toml_config::domain_recently_used() const
    {
        std::filesystem::path path = "";
        auto data = toml::parse(m_filepath);
        if (data.contains("domain") && data.at("domain").contains("recently_used"))
        {
            path = toml::find<std::string>(data, "domain", "recently_used");
        }
        return path;
    }

    std::vector<std::filesystem::path> toml_config::domain_locations() const
    {
        std::vector<std::filesystem::path> ret;
        auto data = toml::parse(m_filepath);
        if (data.contains("domain") && data.at("domain").contains("locations"))
        {
            auto v = toml::find<std::vector<std::string>>(data, "domain", "locations");
            for (auto& x : v)
            {
                ret.emplace_back(x);
            }
        }
        return ret;
    }

    bool toml_config::m_keys_exist(const std::vector<std::string>& keys, toml::ordered_value& data)
    {
        if (keys.size() == 1)
        {
            return data.contains(keys[0]);
        }
        return data.contains(keys[0]) && data.at(keys[0]).contains(keys[1]);
    }

    toml::ordered_value &toml_config::m_data_for_keys(const std::vector<std::string> &keys, toml::ordered_value &data)
    {
        if (keys.size() == 1)
        {
            return data.at(keys[0]);
        }
        
        return data.at(keys[0]).at(keys[1]);
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

    void toml_config::m_write(auto& data)
    {
        std::ofstream out(m_filepath);
        out << toml::format(data) << std::endl;
    }

} // namespace sz
