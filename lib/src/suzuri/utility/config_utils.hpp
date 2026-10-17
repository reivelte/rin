// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <string>
#include <string_view>
#include <filesystem>
#include <tuple>
#include <utility>
#include <type_traits>
#include <variant>
#include <toml.hpp>
#include <sz_export.hpp>
#include "../result.hpp"
#include "../basic_vectors.hpp"
#include "string.hpp"

namespace sz
{
    enum class data_directory_type : int
    {
        Config, Data, State, Cache
    };

    enum class config_type : int
    {
        Static, State
    };

    SZ_API std::filesystem::path get_home_directory();
    SZ_API std::filesystem::path get_xdg_directory(data_directory_type type);
    SZ_API std::filesystem::path get_config_path(config_type type);
    SZ_API bool is_valid_domain(const std::filesystem::path& path);

    namespace detail
    {
        // referenced from: https://www.youtube.com/watch?v=EsUmnLgz8QY
        template <typename... Base>
        struct basic_visitor : Base...
        {
            using Base::operator()...;
        };
        template <typename... T> basic_visitor(T...) -> basic_visitor<T...>;

        template <typename... Funcs, typename... Types>
        inline void type_conditional_call(detail::basic_visitor<Funcs...> visitor, Types&&... values)
        {
            using Tuple_Of_Types = decltype(std::tuple<std::remove_cvref_t<Types>...>());
            using T = std::tuple_element_t<0, Tuple_Of_Types>;
            using Ts = typename std::decay_t<decltype((std::forward<Types>(values), ...))>;
            constexpr std::size_t N = sizeof...(Types);

            constexpr bool all_same_type = [&] <std::size_t... Idx> (std::index_sequence<Idx...>) constexpr -> bool
            {
                return ((std::same_as<T, std::tuple_element_t<Idx, Tuple_Of_Types>>) && ...);
            }(std::index_sequence_for<Types...>{});

            if constexpr (N > 1 && all_same_type)
            {
                std::vector<T> x{values...};
                std::variant<std::vector<T>> v;
                v = x;
                std::visit(visitor, v);
            }
            else
            {
                const std::array<std::variant<Ts>, sizeof...(Types)> x{std::variant<Ts>(values)...};
                for (const auto& v : x)
                { std::visit(visitor, v); }
            }
        }
    }

    class SZ_API toml_config
    {
        public:
        static sz::result<toml_config> create_or_open(const std::filesystem::path& p, config_type t);

        toml_config() = default;
        toml_config(const std::filesystem::path& p, config_type t);
        ~toml_config() = default;

        template <typename... Types>
        void set_value(std::string key, Types&&... values);

        sz::result<void> set_config(const std::filesystem::path& p, config_type t);
        
        template <typename... Types>
        auto value(std::string key);

        bool has_value(std::string key) const;
        bool available() const;

        private:
        template <typename... Types>
        void m_initialize_field(std::string subkey, toml::ordered_value& data);

        template <typename T>
        void m_write(T& data);

        sz::result<toml::ordered_value> m_init();
        
        toml::ordered_value& m_data_for_keys(const std::vector<std::string>& keys, toml::ordered_value& data) const;
        bool m_keys_exist(const std::vector<std::string>& keys, toml::ordered_value& data) const;
        sz::result<toml::ordered_value> m_read() const;

        private:
        std::filesystem::path m_filepath;
        config_type m_type;
    };

    // assumes key contains either zero or one subkey(s)
    // supported types: string/string_view, int, float/double, or a vector of these
    template <typename... Types>
    inline void toml_config::set_value(std::string key, Types&&... values)
    {
        if (key.empty())
        { return; }

        auto data = toml::parse<toml::ordered_type_config>(m_filepath);
        const std::vector<std::string> keys = sz::utility::split(".", key);
        
        if (keys.size() == 1) // TODO: figure out how many subkeys we want to support
        { m_initialize_field<Types...>(key, data); }
        else
        { m_initialize_field<Types...>(keys[1], data[keys[0]]); }
        
        #define data_append(DATA, VECTOR) toml::ordered_value& arr = m_data_for_keys(keys, DATA); for (const auto& x : VECTOR) { arr.push_back(x); }

        detail::basic_visitor visitor{
            [&](const std::vector<int>& v) ->           void { data_append(data, v); },
            [&](const std::vector<double>& v) ->        void { data_append(data, v); },
            [&](const std::vector<std::string>& v) ->   void { data_append(data, v); },
            [&](int v) ->                               void { m_data_for_keys(keys, data) = v; },
            [&](double v) ->                            void { m_data_for_keys(keys, data) = v; },
            [&](std::string_view v) ->                  void { m_data_for_keys(keys, data) = v; },
            [&](const std::string& v) ->                void { m_data_for_keys(keys, data) = v; }
        };
        
        #undef data_append

        detail::type_conditional_call(visitor, std::forward<Types>(values)...);
        m_write(data);
    }

    template <typename... Types>
    inline auto toml_config::value(std::string key)
    {
        using Tuple_Of_Types = decltype(std::tuple<std::remove_cvref_t<Types>...>());
        using T = std::tuple_element_t<0, Tuple_Of_Types>;

        constexpr size_t N = sizeof...(Types);

        auto data = toml::parse<toml::ordered_type_config>(m_filepath);
        std::vector<std::string> keys = sz::utility::split(".", key);

        if (!m_keys_exist(keys, data))
        {
            if (keys.size() == 1)
            { m_initialize_field<Types...>(keys[0], data); }
            else
            {
                if (!data.contains(keys[0]))
                { data[keys[0]] = toml::ordered_value(); }
                
                m_initialize_field<Types...>(keys[1], data[keys[0]]);
            }
            m_write(data);
            data = toml::parse<toml::ordered_type_config>(m_filepath);
        }

        detail::basic_visitor visitor{
            [&](std::vector<std::string>& ret) -> void
            {
                auto arr = m_data_for_keys(keys, data).as_array();
                for (const auto& s : arr)
                {
                    ret.emplace_back(s.as_string());
                }
            },
            [&](std::string& ret) -> void
            {
                ret = m_data_for_keys(keys, data).as_string();
            },
            [&](std::tuple<int, int>& ret) -> void
            {
                ret = toml::find<std::tuple<int, int>>(data, keys[0], keys[1]);
            }
        };

        if constexpr (N == 1)
        {
            std::variant<T> ret{};
            std::visit(visitor, ret);
            return std::get<0>(ret);
        }
        else
        {
            // TODO: this is duplicated code from type_conditional_call()
            constexpr bool all_same_type = [&] <std::size_t... Idx> (std::index_sequence<Idx...>) constexpr -> bool
            {
                return ((std::same_as<T, std::tuple_element_t<Idx, Tuple_Of_Types>>) && ...);
            }(std::index_sequence_for<Types...>{});

            if constexpr (all_same_type)
            {
                std::variant<std::tuple<Types...>> ret;
                std::visit(visitor, ret);
                return std::get<0>(ret);
            }
        }
    }

    template <typename... Types>
    inline void toml_config::m_initialize_field(std::string subkey, toml::ordered_value& data)
    {
        using Tuple_Of_Types = decltype(std::tuple<std::remove_cvref_t<Types>...>());
        auto visitor = [&] <std::size_t... Idx> (std::index_sequence<Idx...>) -> auto
        {
            // TODO: use parameter pack indexing here after switching to C++26
            #define T(INDEX) std::tuple_element_t<INDEX, Tuple_Of_Types>
            return detail::basic_visitor{
                ([&](const T(Idx)& v) -> void
                {
                    (void)v;
                    data[subkey] = T(Idx)();
                },...),
                ([&](const std::vector<T(Idx)>& v) -> void
                {
                    (void)v;
                    data[subkey] = toml::array();
                },...)
            };
            #undef T
        }(std::index_sequence_for<Types...>{});
        detail::type_conditional_call(visitor, (std::remove_cvref_t<Types>())...);
    }

    template <typename T>
    void toml_config::m_write(T& data)
    {
        std::ofstream out(m_filepath);
        out << toml::format(data) << std::endl;
    }
} // namespace sz
