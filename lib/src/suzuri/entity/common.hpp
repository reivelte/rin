// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>
#include <sz_export.hpp>
#include "../types.hpp"

namespace sz
{
    // an entity id is a platform agnostic path without a prefix
    // an entity uri is a platform agnostic path with a prefix
    // an entity platform id (pid) is a platform specific path without a prefix

    constexpr int ENTITY_TOTAL_COLUMN_COUNT = 4;
    constexpr int ENTITY_MUTABLE_COLUMN_COUNT = 3;

    constexpr int FILE_ENTITY_TYPE = 3;
    constexpr int TAG_ENTITY_TYPE = 4;
    constexpr int ALIAS_ENTITY_TYPE = 5;
    constexpr int CONCEPT_ENTITY_TYPE = 7;

    constexpr int FILE_ENTITY_NUM_EXTRA_FIELDS = 2;
    constexpr int TAG_ENTITY_NUM_EXTRA_FIELDS = 0;
    constexpr int ALIAS_ENTITY_NUM_EXTRA_FIELDS = 1;

    enum class entity_type : i8
    {
        Invalid = -1,
        File = FILE_ENTITY_TYPE,
        Tag = TAG_ENTITY_TYPE,
        Alias = ALIAS_ENTITY_TYPE,
        Concept = CONCEPT_ENTITY_TYPE
    };

    enum class entity_attribute_type : i32
    {
        // applies to all entities
        Null_Attribute = 0,
        Name = 1,               // string
        Id,                     // string
        Parent_Id,              // string
        Tags,                   // set<string>
        Comments, Description,  // string
        Color,                  // int
        Url,                    // QUrl
        Thumbnail,

        // applies to filesystem based entities
        Size, Modified, Created, Accessed, File_Type,       // string
        Size_Int, Modified_Int, Created_Int, Accessed_Int,  // qint64
        Rating,                                             // int
        Icon,                                               // QIcon
        File_Info,                                          // QFileInfo
        Mime_Type,                                          // QMimeType

        // applies to filesystem based image entities
        Sizehint,               // tuple<int, int> or similar

        // TODO: fields for specfic file types and entities
    };

    typedef size_t entity_index;
    typedef size_t attribute_index;

    static constexpr const std::string_view FILE_URI_PREFIX =    "file://";
    static constexpr const std::string_view INDEX_URI_PREFIX =   "index://";
    static constexpr const std::string_view CONCEPT_URI_PREFIX = "concept://";
    static constexpr const std::string_view HTTPS_URI_PREFIX =   "https://";

    SZ_API bool is_valid_tag(std::string_view tag);
    SZ_API std::string entity_uri(const std::string& id, entity_type type);
    SZ_API std::tuple<std::string_view, sz::entity_type> id_from_uri(std::string_view uri);
    SZ_API std::string entity_platform_id(const std::string& id, entity_type type);
    
} // namespace sz
