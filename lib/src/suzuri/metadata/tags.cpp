// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <ranges>
#include <fstream>
#include <iostream>
#include "tags.hpp"
#include "../utility/json.hpp"
#include "../utility/string.hpp"

namespace sz::metadata
{
    std::vector<std::string> get_tags(const std::filesystem::path& tagfile_path, const std::vector<std::string>& keys, const std::string& split_on)
    {
        std::vector<std::string> tags;
        if (keys.empty() || tagfile_path.extension() == ".txt")
        {
            std::ifstream f(tagfile_path);
            
            if (!f)
            { return tags; }

            std::string content{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
            auto trim_range = [](auto&& range) -> std::string
            {
                std::string_view sv(&*range.begin(), std::ranges::distance(range));
                
                if (!sv.empty() && sv.back() == '\r')
                { sv.remove_suffix(1); }

                return std::string(utility::trim(sv));
            };
            
            for (auto&& line_range : content | std::views::split('\n') | std::views::transform(trim_range))
            { tags.emplace_back(std::move(line_range)); }

        }
        else if (tagfile_path.extension() == ".json")
        {
            nlohmann::json j = utility::json_from_file(tagfile_path);

            if (j.empty())
            { return tags;}

            for (const auto& key : keys)
            {
                if (j.contains(key))
                {
                    std::string s = j[key];
                    tags.append_range(utility::split(split_on, s));
                }
            }
        }

        return tags;
    }
}


