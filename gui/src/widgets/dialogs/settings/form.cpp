// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "form.hpp"
#include "dialog.hpp"

namespace rin
{
    settings_form::settings_form(settings_dialog* parent) :
        QFrame(parent)
    {
        connect(parent, &settings_dialog::settings_applied, this, &settings_form::commit);
    }
} // namespace rin
