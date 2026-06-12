// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <suzuri/result.hpp>
#include <suzuri/utility/json.hpp>

namespace rin
{
    struct program_configuration
    {
        std::filesystem::path database_path;
    };

    program_configuration read_config(const nlohmann::json& json);

    sz::result<nlohmann::json> make_default_config_json(const std::filesystem::path& target_path);

    sz::result<nlohmann::json> get_default_config();
}