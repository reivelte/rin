// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <suzuri/result.hpp>
#include <suzuri/utility/config_utils.hpp>

namespace rin
{
    sz::result<sz::toml_config> get_default_config();
}