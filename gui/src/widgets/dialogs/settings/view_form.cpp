// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <suzuri/core/ctl.hpp>
#include "view_form.hpp"

namespace rin
{
    view_settings_form::view_settings_form(settings_dialog* parent) :
        settings_form(parent)
    {
        m_form_layout = new QFormLayout(this);
        m_visual_align_chooser = new QComboBox(this);
        m_visual_align_chooser->addItems({"Top", "Center", "Bottom"});

        m_form_layout->addRow("Item Row Alignment", m_visual_align_chooser);
    }

    view_settings_form::~view_settings_form()
    {
    }

    bool view_settings_form::can_apply_settings() const
    {
        return true;
    }

    void view_settings_form::commit()
    {
        const auto value = m_visual_align_chooser->currentText().toLower().toStdString();
        SZ_MAIN_CONF->set_value("view.item_row_alignment", value);
    }

} // namespace rin
