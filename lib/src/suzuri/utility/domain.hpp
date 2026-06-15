// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <filesystem>
#include "config_utils.hpp"

namespace sz::utility
{
    SZ_API bool create_domain(const std::filesystem::path& path);
    SZ_API std::filesystem::path default_domain_path();
    SZ_API std::filesystem::path database_path_from_domain_path(const std::filesystem::path& domain_path);
    
} // namespace sz::utility
