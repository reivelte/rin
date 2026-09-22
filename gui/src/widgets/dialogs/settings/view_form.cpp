// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "view_form.hpp"

namespace rin
{
    view_settings_form::view_settings_form(const std::shared_ptr<sz::toml_config>& config, settings_dialog* parent) :
        settings_form(parent), m_config(config)
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
        m_config->set_value("view.item_row_alignment", value);
    }

} // namespace rin
