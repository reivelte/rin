// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "../entity/common.hpp"
#include "config_utils.hpp"
#include "hash.hpp"
#include "files.hpp"
#include "string.hpp"
#include "thumbnail.hpp"

namespace sz
{
    int from_thumbnail_target_size(thumbnail_target_size s)
    {
        std::string as = utility::to_string(s);
        const size_t first = as.find('_') + 1;
        const size_t last = as.rfind("p");
        return std::stoi(as.substr(first, last - first));
    }

    thumbnail_target_size to_thumbnail_target_size(int w, int h)
    {
        using enum sz::thumbnail_target_size;

        if (w <= 128 && h <= 128)
        { return Normal_128px; }
        else if (w <= 256 && h <= 256)
        { return Large_256px; }
        else if (w <= 512 && h <= 512)
        { return Large_512px; }
        else if (w <= 1024 && h <= 1024)
        { return Large_1024px; }
        
        return Normal_128px;
    }

    std::string_view thumbnail_target_size_to_subdirectory_name(thumbnail_target_size s)
    {
        using enum thumbnail_target_size;
        switch (s)
        {
        default:
        case Normal_128px:  { return sz::THUMBNAIL_CACHE_SUBDIRECTORY_NORMAL; }
        case Large_256px:   { return sz::THUMBNAIL_CACHE_SUBDIRECTORY_LARGE; }
        case Large_512px:   { return sz::THUMBNAIL_CACHE_SUBDIRECTORY_XLARGE; }
        case Large_1024px:  { return sz::THUMBNAIL_CACHE_SUBDIRECTORY_XXLARGE; }
        }
        return {};
    }

    std::filesystem::path thumbnail_cache_path()
    {
        return std::filesystem::path(sz::get_xdg_directory(sz::data_directory_type::Cache)) / "thumbnails";
    }
    
    // does not check if the returned path points to an existing file in the cache
    std::filesystem::path thumbnail_cache_path(std::string_view entity_uri, thumbnail_target_size size)
    {
        if (entity_uri.starts_with(sz::FILE_URI_PREFIX))
        { entity_uri.remove_prefix(sz::FILE_URI_PREFIX.size()); }

        return thumbnail_cache_path() / thumbnail_target_size_to_subdirectory_name(size)
        / (sz::utility::hash(entity_uri, sz::utility::hash_type::MD5) + ".png");
    }
    
    std::unordered_set<std::string_view> supported_extensions()
    {
        static constexpr const std::array<std::string_view, 4> s_valid_thumbnail_types{
            "jpg", "jpeg", "png", "bmp" 
        };

        std::unordered_set<std::string_view> ret;

        for (const auto& ext : s_valid_thumbnail_types)
        { ret.emplace(ext); }

        return ret;
    }
} // namespace sz
