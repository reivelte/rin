// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <filesystem>
#include <unordered_set>
#include <sz_export.hpp>
#include "../result.hpp"
#include "../utility/shared_library.hpp"

namespace sz
{
    struct SZ_API plugin_descriptor
    {
        std::string name;
        std::string type;
        std::string version;
        std::unordered_set<std::string> features;

        std::unordered_set<std::string> subfeatures(std::string key) const;
    };

    // the base class from which all plugins should inherit
    class SZ_API native_plugin
    {
        public:
        // caller takes ownership of the returned pointer and is responsible for calling delete on it
        static result<native_plugin*> from_shared_library(const sz::shared_library& lib, int plugin_index);

        native_plugin() = default;
        virtual ~native_plugin() = default;

        virtual plugin_descriptor descriptor() const = 0;
    };

    class SZ_API thumbnailer : public native_plugin
    {
        public:
        thumbnailer() = default;
        virtual ~thumbnailer() = default;

        // caller guarantees that path points to an existing file on the filesystem
        // width and height are the expected dimensions for the requested thumbnail
        virtual std::string create_thumbnail(std::filesystem::path path, int width, int height) = 0;
    };
    
} // namespace sz
