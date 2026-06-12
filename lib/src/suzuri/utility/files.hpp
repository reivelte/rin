// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <cstdint>
#include <filesystem>
#include <sz_export.hpp>
#include "../result.hpp"

namespace sz::utility
{
    struct SZ_API file_stat
    {
        int64_t modtime;
        uintmax_t size;
        file_stat() = default;
        file_stat(int64_t m, uintmax_t s) : modtime(m), size(s) {}
        auto operator<=>(const file_stat&) const = default;
    };

    SZ_API result<file_stat> stat(const std::filesystem::path& path);
}