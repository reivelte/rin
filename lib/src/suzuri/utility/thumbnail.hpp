// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <filesystem>
#include <string>
#include <unordered_set>
#include <sz_export.hpp>

namespace sz
{
    enum class thumbnail_target_size : int
    {
        Normal_128px, // normal
        Large_256px,  // large
        Large_512px,  // x-large
        Large_1024px  // xx-large
    };

    static constexpr const std::string_view THUMBNAIL_CACHE_SUBDIRECTORY_NORMAL = "normal";
    static constexpr const std::string_view THUMBNAIL_CACHE_SUBDIRECTORY_LARGE = "large";
    static constexpr const std::string_view THUMBNAIL_CACHE_SUBDIRECTORY_XLARGE = "x-large";
    static constexpr const std::string_view THUMBNAIL_CACHE_SUBDIRECTORY_XXLARGE = "xx-large";

    SZ_API int from_thumbnail_target_size(thumbnail_target_size s);
    SZ_API thumbnail_target_size to_thumbnail_target_size(int w, int h);
    SZ_API std::string_view thumbnail_target_size_to_subdirectory_name(thumbnail_target_size s);
    SZ_API std::filesystem::path thumbnail_cache_path();
    SZ_API std::filesystem::path thumbnail_cache_path(std::string_view entity_uri, thumbnail_target_size size);
    SZ_API std::unordered_set<std::string_view> supported_extensions();

} // namespace sz
