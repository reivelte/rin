// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "dialog.hpp"
#include "general_form.hpp"
#include "navigation_form.hpp"
#include "view_form.hpp"

namespace rin
{
    settings_dialog::settings_dialog(const std::shared_ptr<sz::toml_config>& config, QWidget* parent)
    : QDialog(parent), m_config(config)
    {
        m_menulist = new QListWidget(this);
        m_pages = new QStackedWidget(this);
        
        m_add_form(tr("General"), new general_settings_form(config, this));
        m_add_form(tr("Navigation"), new navigation_panel_settings_form(config, this));
        m_add_form(tr("View"), new view_settings_form(config, this));

        // buttons
        m_button_box = new QDialogButtonBox(
            QDialogButtonBox::StandardButton::Cancel | QDialogButtonBox::StandardButton::Ok,
            Qt::Orientation::Horizontal,
            this
        );

        m_apply_button = m_button_box->addButton(QDialogButtonBox::StandardButton::Apply);
        
        m_hsplit = new QSplitter;
        m_hsplit->addWidget(m_menulist);
        m_hsplit->addWidget(m_pages);
        
        m_vbox = new QVBoxLayout(this);
        m_vbox->addWidget(m_hsplit);
        m_vbox->addWidget(m_button_box);

        connect(m_menulist, &QListWidget::itemClicked, this, &settings_dialog::change_page);
        connect(m_button_box, &QDialogButtonBox::accepted, this, &settings_dialog::apply_settings_and_accept);
        connect(m_button_box, &QDialogButtonBox::rejected, this, &settings_dialog::reject);
        connect(m_button_box, &QDialogButtonBox::clicked, this, &settings_dialog::handle_button_click);
    }

    settings_dialog::~settings_dialog()
    {
    }

    settings_form* settings_dialog::page(const QString& name) const
    {
        if (m_forms.contains(name))
        { return m_forms.at(name); }

        return nullptr;
    }

    void settings_dialog::change_page(QListWidgetItem* item)
    {
        if (const QString name = item->text(); m_forms.contains(name))
        { m_pages->setCurrentWidget(m_forms[name]); }
    }

    bool settings_dialog::apply_settings()
    {
        if (auto* form = qobject_cast<settings_form*>(m_pages->currentWidget()); form->can_apply_settings())
        {
            form->commit();
            emit settings_applied();
            return true;
        }
        return false;
    }

    void settings_dialog::apply_settings_and_accept()
    {
        if (apply_settings())
        {
            accept();
        }
    }

    void settings_dialog::m_add_form(const QString& name, settings_form* form)
    {
        m_menulist->addItem(name);
        m_pages->addWidget(form);
        m_forms.emplace(name, form);
    }

    void settings_dialog::handle_button_click(QAbstractButton* button)
    {
        if (button == m_apply_button)
        {
            apply_settings();
        }
    }
} // namespace rin
