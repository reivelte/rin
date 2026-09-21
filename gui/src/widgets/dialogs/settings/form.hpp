// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <memory>
#include <QtWidgets/QFrame>
#include <suzuri/utility/config_utils.hpp>

namespace rin
{
    class settings_dialog;

    class settings_form : public QFrame
    {
        Q_OBJECT

        public:
        settings_form(settings_dialog* parent);
        ~settings_form() = default;

        virtual bool can_apply_settings() const = 0;

        public slots:
        virtual void commit() = 0;
    };
} // namespace rin
