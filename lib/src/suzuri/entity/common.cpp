// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <algorithm>
#include "../result.hpp"
#include "common.hpp"

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

    std::string entity_uri(const std::string& id, entity_type type)
    {
        using enum entity_type;
        std::string prefix;
        switch (type)
        {
        default:
        case Concept:   { prefix = CONCEPT_URI_PREFIX; break; }
        case File:      { prefix = FILE_URI_PREFIX; break; }
        case Alias:
        case Tag:       { prefix = TAG_URI_PREFIX; break; }
        }
        return prefix + id;
    }

    std::string entity_platform_id(const std::string& id, entity_type type)
    {
        #ifdef _WIN32
        if (type == entity_type::File)
        {
            // get the correct drive letter
            // TODO
        }
        return id;
        #else
        (void)type;
        return id;
        #endif
    }

} // namespace sz
