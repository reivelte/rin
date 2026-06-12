// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <memory>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <suzuri/metadata/tags.hpp>

namespace rin
{
    class sidecar_dialog : public QDialog
    {
        Q_OBJECT

        public:
        sidecar_dialog(QString items_root, QWidget* parent);
        ~sidecar_dialog();

        void set_items_root(QString path);
        void set_sidecar_root(QString path);

        QString items_root() const;
        QString sidecar_root() const;
        QString sidecar_split_string() const;
        QString sidecar_json_key() const;
        sz::metadata::sidecar_filetype sidecar_type() const;

        private:
        QLineEdit* m_items_root_lineedit;
        QLineEdit* m_sidecar_root_lineedit;
        QLineEdit* m_sidecar_split_string_lineedit;
        QLineEdit* m_sidecar_json_tags_key_lineedit;
        QComboBox* m_sidecar_type_selector;
        QDialogButtonBox* m_buttons;
        QFormLayout* m_layout;
    };
    
} // namespace rin
