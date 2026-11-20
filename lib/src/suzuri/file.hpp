// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <string>
#include <filesystem>
#include <sz_export.hpp>
#include "utility/shared_data.hpp"
#include "types.hpp"
#include "result.hpp"

namespace sz
{
    class SZ_API file
    {
        public:
        file();
        explicit file(std::filesystem::path path);
        ~file();

        operator std::string() const;
        operator std::filesystem::path() const;

        std::filesystem::path path() const;
        std::filesystem::path name() const;
        std::filesystem::path suffix() const;
        std::string extension() const;

        result<bool> exists() const;
        result<bool> is_file() const;
        result<bool> is_dir() const;
        result<i64> modtime() const;

        private:
        struct data
        {
            std::filesystem::path path;
        }; 
        utility::shared_implicit<data> m;
    };
    
} // namespace sz
