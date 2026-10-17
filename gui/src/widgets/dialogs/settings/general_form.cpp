// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <QtCore/QFileInfo>
#include <suzuri/core/ctl.hpp>
#include "general_form.hpp"

namespace rin
{
    general_settings_form::general_settings_form(settings_dialog* parent) :
        settings_form(parent)
    {
        m_form_layout = new QFormLayout(this);
        m_startfolder_lineedit = new QLineEdit;

        m_form_layout->addRow(tr("&Start Folder:"), m_startfolder_lineedit);

        if (auto start_folder = SZ_MAIN_CONF->value<std::string>("general.start_folder"); start_folder.size())
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
            SZ_MAIN_CONF->set_value("general.start_folder", path.toStdString());
        }
    }
} // namespace rin
