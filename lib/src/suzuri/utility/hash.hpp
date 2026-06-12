// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <string>
#include <string_view>
#include <filesystem>
#include <sz_export.hpp>

namespace sz::utility
{
    SZ_API std::string sha256(std::string_view in);
    SZ_API std::string sha256_from_stat(const std::filesystem::path& path);

}