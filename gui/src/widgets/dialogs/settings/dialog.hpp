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
    class settings_form;
    class settings_dialog : public QDialog
    {
        Q_OBJECT
        
        public:
        settings_dialog(const std::shared_ptr<sz::toml_config>& config, QWidget* parent);
        ~settings_dialog();

        settings_form* page(const QString& name) const;
        settings_form* current_page() const;
        QString current_page_name() const;

        signals:
        void settings_applied();

        public slots:
        void change_page(QListWidgetItem* page_name_widget);
        bool apply_settings();
        void apply_settings_and_accept();

        protected slots:
        void handle_button_click(QAbstractButton* button);

        private:
        void m_add_form(const QString& name, settings_form* form);

        private:
        std::shared_ptr<sz::toml_config> m_config;
        std::unordered_map<QString, settings_form*> m_forms;
        QString m_current_page;
        QListWidget* m_menulist;
        QStackedWidget* m_pages;
        QSplitter* m_hsplit;
        QVBoxLayout* m_vbox;
        QDialogButtonBox* m_button_box;
        QPushButton* m_apply_button;

    };
} // namespace rin
