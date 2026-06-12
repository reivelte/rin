// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <memory>
#include <QtWidgets/QDialog>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QFileDialog>
#include <suzuri/utility/config_utils.hpp>

namespace rin
{
    class settings_dialog;

    class general_settings_form : public QFrame
    {
        Q_OBJECT

        public:
        general_settings_form(const std::shared_ptr<sz::toml_config>& config, settings_dialog* parent);
        ~general_settings_form();

        bool can_apply_settings() const;

        public slots:
        void handle_settings_applied();

        private:
        std::shared_ptr<sz::toml_config> m_config;
        QFormLayout* m_form_layout;
        QLineEdit* m_startfolder_lineedit;
    };

    class navigation_panel_settings_form : public QFrame
    {
        Q_OBJECT
        
        public:
        navigation_panel_settings_form(const std::shared_ptr<sz::toml_config>& config, settings_dialog* parent);
        ~navigation_panel_settings_form() = default;

        QList<QFileInfo> targets();

        bool can_apply_settings() const;

        signals:
        void new_navigation_panel_targets_applied();
        void new_navigation_panel_targets(const QList<QFileInfo>& targets);
        
        public slots:
        void add_path_from_lineedit();
        void purge_item(QListWidgetItem* item);
        void handle_button_click(bool);
        void handle_settings_applied();

        private:
        std::shared_ptr<sz::toml_config> m_config;
        QFormLayout* m_form_layout;
        QListWidget* m_items;
        
        QHBoxLayout* m_path_input_layout;
        QLineEdit* m_path_lineedit;
        QPushButton* m_path_chooser_button;
        
        QLineEdit* m_home_lineedit;
        // QDialogButtonBox* m_button_box;
    };

    class settings_dialog : public QDialog
    {
        Q_OBJECT
        
        public:
        settings_dialog(const std::shared_ptr<sz::toml_config>& config, QWidget* parent);
        ~settings_dialog();

        general_settings_form* general_settings_page() const { return m_general_settings; }
        navigation_panel_settings_form* navigation_settings_page() const { return m_navpanel_settings; }

        signals:
        void settings_applied();

        public slots:
        void change_page(QListWidgetItem* page_name_widget);
        bool apply_settings();
        void apply_settings_and_accept();

        protected slots:
        void handle_button_click(QAbstractButton* button);

        private:
        std::shared_ptr<sz::toml_config> m_config;
        // QHBoxLayout* m_hbox;

        general_settings_form* m_general_settings;
        navigation_panel_settings_form* m_navpanel_settings;

        QSplitter* m_hsplit;
        QVBoxLayout* m_vbox;
        QListWidget* m_menulist;
        QStackedWidget* m_pages;
        QDialogButtonBox* m_button_box;
        QPushButton* m_apply_button;

    };
    
} // namespace rin
