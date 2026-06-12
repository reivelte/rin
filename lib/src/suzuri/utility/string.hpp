// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <sz_export.hpp>
#include "../result.hpp"

namespace sz::utility
{
    template <typename T> requires (std::constructible_from<std::string, T> || std::is_same_v<char, T>)
    SZ_API constexpr std::string join(const std::string& on, const std::vector<T>& v, bool join_always = false)
    {
        std::string r = "";
        if constexpr (std::is_same_v<char, T>) for (const auto& c : v) r += c + on;
        else for (const auto& s : v) r += std::string(s) + on;
        return join_always ? r : r.substr(0, r.size() - on.size());
    }

    SZ_API constexpr std::string_view trim(std::string_view s)
    {
        auto front = std::find_if_not(s.begin(), s.end(), [](char c){ return std::isspace(c); });
        auto back = std::find_if_not(s.rbegin(), s.rend(), [](char c){ return std::isspace(c); }).base();
        return (front < back) ? std::string_view(front, back) : std::string_view{};
    }
    
    SZ_API constexpr std::string_view trim_r(std::string_view s, char what)
    {
        auto front = s.cbegin();
        auto back = std::find_if_not(s.rbegin(), s.rend(), [&](char c){ return c == what; }).base();
        return (front < back) ? std::string_view(front, back) : std::string_view{};
    }
    
    SZ_API std::vector<std::string> split(std::string_view on, std::string_view s);

    // is hex string
    SZ_API bool isxstr(std::string_view s);

    // hex string to int
    SZ_API result<int> hstoi(std::string_view s);

    // string to int
    SZ_API result<int> stoi(std::string_view s);
    
}