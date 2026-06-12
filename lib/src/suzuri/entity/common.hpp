// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <cstdint>
#include <string>
#include <sz_export.hpp>

namespace sz
{
    constexpr int ENTITY_TOTAL_COLUMN_COUNT = 4;
    constexpr int ENTITY_MUTABLE_COLUMN_COUNT = 3;

    constexpr int FILE_ENTITY_TYPE = 3;
    constexpr int TAG_ENTITY_TYPE = 4;
    constexpr int ALIAS_ENTITY_TYPE = 5;
    constexpr int CONCEPT_ENTITY_TYPE = 7;

    constexpr int FILE_ENTITY_NUM_EXTRA_FIELDS = 2;
    constexpr int TAG_ENTITY_NUM_EXTRA_FIELDS = 0;
    constexpr int ALIAS_ENTITY_NUM_EXTRA_FIELDS = 1;

    enum class entity_type : int
    {
        Invalid = -1,
        File = FILE_ENTITY_TYPE,
        Tag = TAG_ENTITY_TYPE,
        Alias = ALIAS_ENTITY_TYPE,
        Concept = CONCEPT_ENTITY_TYPE
    };

    typedef size_t entity_index;
    typedef size_t attribute_index;

    SZ_API bool is_valid_tag(const std::string& tag);
    
} // namespace sz
