// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QtWidgets/QLineEdit>
#include <QtWidgets/QFormLayout>
#include "form.hpp"

namespace rin
{
    class general_settings_form : public settings_form
    {
        Q_OBJECT

        public:
        general_settings_form(const std::shared_ptr<sz::toml_config>& config, settings_dialog* parent);
        ~general_settings_form();

        bool can_apply_settings() const override;

        public slots:
        void commit() override;

        private:
        std::shared_ptr<sz::toml_config> m_config;
        QFormLayout* m_form_layout;
        QLineEdit* m_startfolder_lineedit;
    };
} // namespace rin
