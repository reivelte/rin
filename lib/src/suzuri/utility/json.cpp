// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <fstream>
#include "json.hpp"

namespace sz::utility
{
    nlohmann::json json_from_file(const std::filesystem::path& path)
    {
        std::ifstream f(path);
        if (!f)
        { return {}; }
        return nlohmann::json::parse(f, /*callback*/nullptr, /*allow_exceptions*/false, /*ignore_comments*/true, /*ignore_trailing_commas*/false);
    }
}
