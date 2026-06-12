// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <type_traits>
#include <concepts>
#include <expected>
#include <string_view>
#include <vector>

namespace sz 
{
    template <typename T>
    concept is_result = requires(T t) 
    {
        typename T::value_type;
        typename T::error_type;
        requires std::is_constructible_v<T, bool>; // bool must be constructible from T
        requires std::same_as<std::remove_cvref_t<decltype(*t)>, typename T::value_type>; // literal type of t, with references removed must be same as T::value_type
        requires std::constructible_from<T, std::unexpected<typename T::error_type>>;   // T can be constructed from an std::unexpected of T::error_type
    };

    template <typename T>
    concept is_int_compatible = 
        std::is_enum_v<T> || 
        std::is_convertible_v<std::underlying_type_t<T>, int>  || 
        std::convertible_to<T, int>;

    template <typename T>
    concept is_vector = requires(T t)
    {
        typename std::decay_t<T>::value_type;
        typename std::vector<typename std::decay_t<T>::value_type>;
        requires std::same_as<std::decay_t<T>, std::vector<typename std::decay_t<T>::value_type>>;
    };

    template <typename Func, typename... Args>
    concept is_indicating_function = requires 
    {
        requires std::invocable<Func, Args...> && std::same_as<std::invoke_result_t<Func, Args...>, bool>;
    };

    template <typename T>
    consteval std::string_view type_name() 
    {
    #if defined(__clang__) || defined(__GNUC__)
        std::string_view p = __PRETTY_FUNCTION__;
        std::string_view prefix = "T = ";
        std::string_view suffix = ";";
        auto start = p.find(prefix) + prefix.size();
        auto end = p.find(suffix, start);
        return p.substr(start, end - start);
    #else
        //static_assert(false, "Unsupported compiler");
        // this is TODO for MSVC
        std::string_view p = __FUNCSIG__;
        return p;
    #endif
    }
}


