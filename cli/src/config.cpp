// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <fstream>
#include "config.hpp"

namespace rin
{
    sz::result<sz::toml_config> get_default_config()
    {
        auto conf_path = std::filesystem::path(sz::get_xdg_directory(sz::data_directory_type::Config)) / "rin" / "config.toml";
        return sz::toml_config::create_or_open(conf_path, sz::config_type::Static);
    }
}
