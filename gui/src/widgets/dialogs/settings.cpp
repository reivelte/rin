// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <QtCore/QFile>
#include "settings.hpp"

namespace rin
{
    general_settings_form::general_settings_form(const std::shared_ptr<sz::toml_config>& config, settings_dialog* parent) :
        QFrame(parent), m_config(config)
    {
        m_form_layout = new QFormLayout(this);
        m_startfolder_lineedit = new QLineEdit;

        m_form_layout->addRow(tr("&Start Folder:"), m_startfolder_lineedit);

        if (auto start_folder = m_config->value<std::string>("general.start_folder"); start_folder.size())
        {
            m_startfolder_lineedit->setText(QString::fromStdString(start_folder));
        }

        connect(parent, &settings_dialog::settings_applied, this, &general_settings_form::handle_settings_applied);
    }

    general_settings_form::~general_settings_form()
    {
    }

    bool general_settings_form::can_apply_settings() const
    {
        return !(m_startfolder_lineedit->hasFocus());
    }

    void general_settings_form::handle_settings_applied()
    {
        auto path = m_startfolder_lineedit->text();
        
        if (QFileInfo info(path); info.isDir())
        {
            m_config->set_value("general.start_folder", path.toStdString());
        }
    }

    navigation_panel_settings_form::navigation_panel_settings_form(const std::shared_ptr<sz::toml_config>& config, settings_dialog* parent) :
        QFrame(parent), m_config(config)
    { 
        m_form_layout = new QFormLayout(this);
        m_items = new QListWidget(this);

        m_path_input_layout = new QHBoxLayout;
        m_path_lineedit = new QLineEdit;
        m_path_chooser_button = new QPushButton(tr("Select Path..."));
        m_path_input_layout->addWidget(m_path_lineedit);
        m_path_input_layout->addWidget(m_path_chooser_button);

        m_home_lineedit = new QLineEdit;

        m_form_layout->addRow(tr("&Directories:"), m_items);
        m_form_layout->addRow(tr("Add Directory:"), m_path_input_layout);

        auto t = m_config->value<std::vector<std::string>>("navigation_panel.targets");
        if (t.size())
        {
            for (const std::string& path : t)
            {
                m_items->addItem(QString::fromStdString(path));
            }
        }

        connect(m_items, &QListWidget::itemActivated, this, &navigation_panel_settings_form::purge_item);
        connect(m_path_lineedit, &QLineEdit::returnPressed, this, &navigation_panel_settings_form::add_path_from_lineedit);
        connect(m_path_chooser_button, &QPushButton::clicked, this, &navigation_panel_settings_form::handle_button_click);
        connect(parent, &settings_dialog::settings_applied, this, &navigation_panel_settings_form::handle_settings_applied);
    }

    QList<QFileInfo> navigation_panel_settings_form::targets()
    {
        QList<QFileInfo> targets;
        for (int i = 0; i < m_items->count(); ++i)
        {
            // we expect to only get directory paths here
            auto* x = m_items->item(i);
            targets.emplace_back(x->text());
        }
        return targets;
    }

    bool navigation_panel_settings_form::can_apply_settings() const
    {
        return !(m_path_lineedit->hasFocus());
    }

    void navigation_panel_settings_form::purge_item(QListWidgetItem* item)
    {
        const int row = m_items->row(item);
        m_items->removeItemWidget(item);
        delete m_items->takeItem(row);
    }

    // TODO: all QFileDialogs need to use non-blocking show() instead of exec()
    void navigation_panel_settings_form::handle_button_click(bool checked)
    {
        Q_UNUSED(checked);
        QFileDialog dialog(this, tr("Select path"));
        dialog.setFileMode(QFileDialog::FileMode::Directory);
        if (dialog.exec())
        {
            const QString path = dialog.selectedFiles()[0];
            m_items->addItem(path);
        }
    }

    void navigation_panel_settings_form::handle_settings_applied()
    {
        auto t = targets();
        std::vector<std::string> paths;
        for (auto& info : t)
        {
            paths.emplace_back(info.absoluteFilePath().toStdString());
        }
        m_config->set_value("navigation_panel.targets", paths);
        emit new_navigation_panel_targets_applied();
    }

    void navigation_panel_settings_form::add_path_from_lineedit()
    {
        const QString path = m_path_lineedit->text();
        if (QFileInfo info(path); info.isDir())
        {
            m_items->addItem(path);
            m_path_lineedit->clear();
        }
    }

    settings_dialog::settings_dialog(const std::shared_ptr<sz::toml_config>& config, QWidget* parent)
    : QDialog(parent), m_config(config)
    {
        m_menulist = new QListWidget(this);
        m_menulist->addItem("General");
        m_menulist->addItem(tr("Navigation"));
        
        m_pages = new QStackedWidget(this);

        m_general_settings = new general_settings_form(config, this);
        m_navpanel_settings = new navigation_panel_settings_form(config, this);
        m_pages->addWidget(m_general_settings);
        m_pages->addWidget(m_navpanel_settings);

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

    void settings_dialog::change_page(QListWidgetItem* page_name_widget)
    {
        auto text = page_name_widget->text();
        qDebug() << "[settings dialog] change page to: " << text;

        // TODO: we should store enums inside the list items instead of comparing strings
        if (text == "General")
        {
            m_pages->setCurrentWidget(m_general_settings);
        }
        else if (text == "Navigation")
        {
            m_pages->setCurrentWidget(m_navpanel_settings);
        }
    }

    bool settings_dialog::apply_settings()
    {
        if (m_pages->currentWidget() == m_general_settings)
        {
            if (!m_general_settings->can_apply_settings())
            { return false; }
        }
        else if (m_pages->currentWidget() == m_navpanel_settings)
        {   
            if (!m_navpanel_settings->can_apply_settings())
            { return false; }
        }
        qDebug() << "[settings dialog] apply settings";
        emit settings_applied();
        return true;
    }

    void settings_dialog::apply_settings_and_accept()
    {
        if (apply_settings())
        {
            accept();
        }
    }

    void settings_dialog::handle_button_click(QAbstractButton* button)
    {
        if (button == m_apply_button)
        {
            apply_settings();
        }
    }

} // namespace rin
