// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#ifndef _WIN32
#include <dlfcn.h>
#else
#include <windows.h>
#endif

#include <string_view>
#include <filesystem>
#include <sz_export.hpp>
#include "../result.hpp"
#include "reference_counter.hpp"

namespace sz
{
    class SZ_API shared_library
    {
        public:
        shared_library();
        shared_library(shared_library&& rhs);
        shared_library(const shared_library& rhs);
        explicit shared_library(const std::filesystem::path& path);
        ~shared_library();

        shared_library& operator=(shared_library&& rhs);
        shared_library& operator=(const shared_library& rhs);
        inline explicit operator bool() const;

        result<void*> load_symbol(std::string_view name) const;

        private:
        void m_init(const std::filesystem::path& path);
        void m_close();

        private:
        void* m_handle;
        reference_counter m_ref;
        std::filesystem::path m_path;
    };

    inline shared_library::operator bool() const
    {
#ifndef _WIN32
        return m_handle != NULL;
#else
        return m_handle != nullptr;
#endif
    }

} // namespace sz
