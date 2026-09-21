// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QtWidgets/QFormLayout>
#include <QtWidgets/QComboBox>
#include "form.hpp"

namespace rin
{
    class view_settings_form : public settings_form
    {
        Q_OBJECT

        public:
        view_settings_form(const std::shared_ptr<sz::toml_config>& config, settings_dialog* parent);
        ~view_settings_form();

        bool can_apply_settings() const override;
        
        public:
        void commit() override;

        private:
        std::shared_ptr<sz::toml_config> m_config;
        QFormLayout* m_form_layout;
        QComboBox* m_visual_align_chooser;
    };
    
} // namespace rin
