// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "domain.hpp"

namespace sz::utility
{
    bool create_domain(const std::filesystem::path& path)
    {
        std::error_code ec;
        std::filesystem::create_directories(path, ec);
        
        if (ec)
        { return false; }

        return true;
    }

    std::filesystem::path default_domain_path()
    {
        return std::filesystem::path(sz::get_xdg_directory(sz::data_directory_type::Data)) / "rin" / "domains";
    }

    std::filesystem::path database_path_from_domain_path(const std::filesystem::path& domain_path)
    {
        return domain_path / (domain_path.filename().string() + ".db");
    }

} // namespace sz::utility
