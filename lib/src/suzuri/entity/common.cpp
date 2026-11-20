// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <algorithm>
#include "../result.hpp"
#include "common.hpp"

namespace sz
{
    bool is_valid_tag(std::string_view tag)
    {
        return !(
            std::find(tag.begin(), tag.end(), ',') != tag.end() ||
            std::find(tag.begin(), tag.end(), '|') != tag.end() ||
            std::find(tag.begin(), tag.end(), '~') != tag.end()
        );
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
        case Tag:       { prefix = INDEX_URI_PREFIX; break; }
        }
        return prefix + id;
    }

    std::tuple<std::string_view, sz::entity_type> id_from_uri(std::string_view uri)
    {
        using enum entity_type;
        
        size_t subs = 0;
        auto type = Invalid;
        
        if (uri.starts_with(CONCEPT_URI_PREFIX))    { subs = CONCEPT_URI_PREFIX.size(); type = Concept; }
        else if (uri.starts_with(FILE_URI_PREFIX))  { subs = FILE_URI_PREFIX.size(); type = File; }
        else if (uri.starts_with(INDEX_URI_PREFIX))   { subs = INDEX_URI_PREFIX.size(); type = Tag; }
        else if (uri.starts_with(HTTPS_URI_PREFIX)) { subs = HTTPS_URI_PREFIX.size(); type = File; }
        // otherwise the uri is invalid

        return { uri.substr(subs), type };
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
