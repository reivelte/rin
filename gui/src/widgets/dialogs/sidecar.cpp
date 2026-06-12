// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "sidecar.hpp"

namespace rin
{
    sidecar_dialog::sidecar_dialog(QString items_root, QWidget* parent) :
        QDialog(parent)
    {
        m_items_root_lineedit = new QLineEdit;
        
        if (items_root.size())
        { m_items_root_lineedit->setText(items_root); }

        m_sidecar_root_lineedit = new QLineEdit;
        m_sidecar_split_string_lineedit = new QLineEdit;
        m_sidecar_json_tags_key_lineedit = new QLineEdit;
        
        m_sidecar_type_selector = new QComboBox;
        m_sidecar_type_selector->addItems({"txt", "json"});

        m_buttons = new QDialogButtonBox(
            QDialogButtonBox::StandardButton::Ok |
            QDialogButtonBox::StandardButton::Cancel
        );

        m_layout = new QFormLayout(this);
        m_layout->addRow(tr("Items Root:"), m_items_root_lineedit);
        m_layout->addRow(tr("Sidecar Root:"), m_sidecar_root_lineedit);
        m_layout->addRow(tr("Sidecar Split String:"), m_sidecar_split_string_lineedit);
        m_layout->addRow(tr("Sidecar Type:"), m_sidecar_type_selector);
        m_layout->addRow(tr("Sidecar JSON Tags Key:"), m_sidecar_json_tags_key_lineedit);
        m_layout->addWidget(m_buttons);

        connect(m_buttons, &QDialogButtonBox::accepted, this, &sidecar_dialog::accept);
        connect(m_buttons, &QDialogButtonBox::rejected, this, &sidecar_dialog::reject);
    }

    sidecar_dialog::~sidecar_dialog()
    {
    }

    void sidecar_dialog::set_items_root(QString path)
    {
        m_items_root_lineedit->setText(path);
    }

    void sidecar_dialog::set_sidecar_root(QString path)
    {
        m_sidecar_root_lineedit->setText(path);
    }

    QString sidecar_dialog::items_root() const
    {
        return m_items_root_lineedit->text();
    }

    QString sidecar_dialog::sidecar_root() const
    {
        return m_sidecar_root_lineedit->text();
    }

    QString sidecar_dialog::sidecar_split_string() const
    {
        return m_sidecar_split_string_lineedit->text();
    }

    QString sidecar_dialog::sidecar_json_key() const
    {
        return m_sidecar_json_tags_key_lineedit->text();
    }

    sz::metadata::sidecar_filetype sidecar_dialog::sidecar_type() const
    {
        auto value = m_sidecar_type_selector->currentText();
        if (value == "txt")
        { return sz::metadata::sidecar_filetype::Plaintext; }
        else if (value == "json")
        { return sz::metadata::sidecar_filetype::Json; }

        return sz::metadata::sidecar_filetype::Json;
    }

} // namespace rin
