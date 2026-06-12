// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <charconv>
#include "string.hpp"

namespace sz::utility
{
    std::vector<std::string> split(std::string_view on, std::string_view s)
    {
        std::vector<std::string> r;
        if (on.empty())
        {
            r.emplace_back(s);
            return r;
        }
        size_t pos = 0;
        for (size_t loc = s.find(on, pos); loc != std::string_view::npos; loc = s.find(on, pos))
        {
            r.emplace_back(s.substr(pos, loc - pos));
            pos = loc + on.size();
        }
        r.emplace_back(s.substr(pos, s.size() - pos));
        return r;
    }

    bool isxstr(std::string_view s)
    {
        if (s.empty() || (s.size() <= 2)) return false;
        if (s.starts_with("0x") || s.starts_with("0X"))
        {
            for (size_t i = 2; i < s.size(); ++i)
            {
                if (!std::isxdigit(s[i])) return false;
            }
            return true;
        }
        return false;
    }

    result<int> hstoi(std::string_view s)
    {
        if (!isxstr(s)) return common_error(result_code::Invalid_Argument);
        s = s.substr(2); // remove leading 0x
        int v{};
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v, 16);
        if (ec != std::errc()) return common_error(result_code::Invalid_Argument);
        return v;
    }
    
    result<int> stoi(std::string_view s)
    {
        int v{};
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
        if (ec != std::errc()) return common_error(result_code::Invalid_Argument);
        return v;
    }
}