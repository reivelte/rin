// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <string>
#include <string_view>
#include <filesystem>
#include <tuple>
#include <toml.hpp>
#include <sz_export.hpp>
#include "../result.hpp"
#include "../basic_vectors.hpp"
#include "string.hpp"

namespace sz
{
    enum class data_directory_type : int
    {
        Config, Data, State
    };

    enum class config_type : int
    {
        Static, State
    };

    SZ_API std::filesystem::path get_home_directory();
    SZ_API std::string get_xdg_directory(data_directory_type type);
    SZ_API bool is_valid_domain(const std::filesystem::path& path);

    class SZ_API toml_config
    {
        public:
        static sz::result<toml_config> create_or_open(const std::filesystem::path& p, config_type t);

        toml_config() = default;
        toml_config(const std::filesystem::path& p, config_type t);
        ~toml_config() = default;

        template <typename T>
        void set_value(std::string key, const T& value);

        template <typename T>
        T value(std::string key);

        bool has_value(std::string key);
        sz::result<void> set_config(const std::filesystem::path& p, config_type t);

        void set_window_size(int w, int h);
        void set_recently_used_domain(const std::filesystem::path& path);
        void add_domain_location(const std::filesystem::path& path);

        inline bool available() const;
        vector2 window_size() const;
        std::filesystem::path domain_recently_used() const;
        std::vector<std::filesystem::path> domain_locations() const;

        private:
        template <typename T>
        void m_initialize_field(std::string subkey, toml::ordered_value& data);

        bool m_keys_exist(const std::vector<std::string>& keys, toml::ordered_value& data);
        toml::ordered_value& m_data_for_keys(const std::vector<std::string>& keys, toml::ordered_value& data);
        
        sz::result<toml::ordered_value> m_init();
        sz::result<toml::ordered_value> m_read() const;
        void m_write(auto& data);

        private:
        std::filesystem::path m_filepath;
        config_type m_type;
    };

    inline bool toml_config::available() const
    {
        if (!std::filesystem::exists(m_filepath))
        { return false; }
        
        if (auto data = m_read(); !data)
        { return false; }

        return true;
    }

    // assumes key contains either zero or one subkey(s)
    template <typename T>
    inline void toml_config::set_value(std::string key, const T& value)
    {
        if (key.empty())
        { return; }

        auto data = toml::parse<toml::ordered_type_config>(m_filepath);
        std::vector<std::string> keys = sz::utility::split(".", key);
        const auto key_count = keys.size();
        
        if (key_count == 1)
        { m_initialize_field<T>(key, data); }
        else
        { m_initialize_field<T>(keys[1], data[keys[0]]); }
        
        if constexpr (std::is_same_v<T, std::vector<std::string>>)
        {
            toml::ordered_value& arr = m_data_for_keys(keys, data);
            const auto& values = static_cast<const std::vector<std::string>&>(value);
            
            for (auto& s : values)
            { arr.push_back(s); }
        }
        else if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view>)
        {
            const auto& value_ = static_cast<const std::string_view&>(value);
            toml::ordered_value& field = m_data_for_keys(keys, data);
            field = value_;
        }

        m_write(data);
    }

    template <typename T>
    inline T toml_config::value(std::string key)
    {
        auto data = toml::parse<toml::ordered_type_config>(m_filepath);
        std::vector<std::string> keys = sz::utility::split(".", key);

        if (!m_keys_exist(keys, data))
        {
            if (keys.size() == 1)
            { m_initialize_field<T>(keys[0], data); }
            else
            {
                if (!data.contains(keys[0]))
                { data[keys[0]] = toml::ordered_value(); }
                
                m_initialize_field<T>(keys[1], data[keys[0]]);
            }
            m_write(data);
            data = toml::parse<toml::ordered_type_config>(m_filepath);
        }

        if constexpr (std::is_same_v<T, std::vector<std::string>>)
        {
            std::vector<std::string> ret;
            auto arr = m_data_for_keys(keys, data).as_array();
            
            for (const auto& s : arr)
            { ret.emplace_back(s.as_string()); }

            return ret;
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            return m_data_for_keys(keys, data).as_string();
        }
        return T();
    }

    template <typename T>
    inline void toml_config::m_initialize_field(std::string subkey, toml::ordered_value& data)
    {
        if constexpr (std::same_as<T, std::vector<std::string>>)
        {
            data[subkey] = toml::array();
        }
        else if constexpr (std::same_as<T, std::string> | std::same_as<T, std::string_view>)
        {
            data[subkey] = std::string();
        }
    }

} // namespace sz
