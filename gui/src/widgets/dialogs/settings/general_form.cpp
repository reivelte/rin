// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <QtCore/QFileInfo>
#include "general_form.hpp"

namespace rin
{
    general_settings_form::general_settings_form(const std::shared_ptr<sz::toml_config>& config, settings_dialog* parent) :
        settings_form(parent), m_config(config)
    {
        m_form_layout = new QFormLayout(this);
        m_startfolder_lineedit = new QLineEdit;

        m_form_layout->addRow(tr("&Start Folder:"), m_startfolder_lineedit);

        if (auto start_folder = m_config->value<std::string>("general.start_folder"); start_folder.size())
        {
            m_startfolder_lineedit->setText(QString::fromStdString(start_folder));
        }
    }

    general_settings_form::~general_settings_form()
    {
    }

    bool general_settings_form::can_apply_settings() const
    {
        return !(m_startfolder_lineedit->hasFocus());
    }

    void general_settings_form::commit()
    {
        auto path = m_startfolder_lineedit->text();
        
        if (QFileInfo info(path); info.isDir())
        {
            m_config->set_value("general.start_folder", path.toStdString());
        }
    }
} // namespace rin
