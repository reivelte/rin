// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <memory>
#include <vector>
#include <filesystem>
#include <QtCore/QString>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>

namespace rin
{
    enum class domain_dialog_type : int
    { New, Open };

    class domain_dialog : public QDialog
    {
        Q_OBJECT

        public:
        domain_dialog(QWidget* parent, domain_dialog_type type);
        ~domain_dialog();

        signals:
        void domain_created(const QString& domain_path);

        protected slots:
        void choose_location();
        void create_domain();

        private:
        // new dialog
        QHBoxLayout* m_name_input_layout;
        QVBoxLayout* m_dialog_layout;
        QDialogButtonBox* m_button_box;
        QLineEdit* m_name_field;
        QLabel* m_name_label;
        QComboBox* m_location_chooser;
        QString m_selected_domain;

        // open dialog

        // common
        domain_dialog_type m_type;
    };
    
} // namespace rin
