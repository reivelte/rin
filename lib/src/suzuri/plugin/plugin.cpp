// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <ranges>
#include "../utility/string.hpp"
#include "plugin.hpp"

namespace sz
{
    std::unordered_set<std::string> plugin_descriptor::subfeatures(std::string key) const
    {
        std::unordered_set<std::string> result;
        for (std::string_view str : features | std::views::filter([&](const std::string& k) -> bool { return k.starts_with(key); }))
        {
            auto subfeatures = utility::split(",", str.substr(key.size() + 1));
            
            if (subfeatures.size() == 1 && subfeatures[0] == key)
            { continue; }
            
            for (const auto& subfeature : subfeatures)
            {
                result.emplace(subfeature);
            }
        }
        return result;
    }

    result<native_plugin*> native_plugin::from_shared_library(const sz::shared_library& lib, int plugin_index)
    {
        native_plugin* (*fn)(int);
        result<void*> symbol = lib.load_symbol("create");

        if (symbol)
        {
            *(void**)(&fn) = *symbol;
            
            if (auto* p = (*fn)(plugin_index); p)
            { return p; }

            return common_error(result_code::No_Action, "module did not provide a plugin");
        }
        return symbol.error();
    }

} // namespace sz
