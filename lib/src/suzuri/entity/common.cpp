// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <algorithm>
#include "common.hpp"
#include "../result.hpp"

namespace sz
{
    bool is_valid_tag(const std::string& tag)
    {
        if (std::find(tag.begin(), tag.end(), ',') != tag.end() ||
            std::find(tag.begin(), tag.end(), '|') != tag.end() ||
            std::find(tag.begin(), tag.end(), '~') != tag.end()) 
        { return false; }
        return true;
    }
    
} // namespace sz
