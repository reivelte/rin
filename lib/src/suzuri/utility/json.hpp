// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>
#include <sz_export.hpp>

namespace sz::utility
{
    SZ_API nlohmann::json json_from_file(const std::filesystem::path& path);
}