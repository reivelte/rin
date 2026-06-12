// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <QtCore/QString>
#include <QtWidgets/QSpinBox>

namespace rin
{
    class icon_sizer : public QSpinBox
    {
        Q_OBJECT

        public:
        explicit icon_sizer(QWidget* parent);

        int valueFromText(const QString& text) const override;
        QString textFromValue(int value) const override;

    };
    
} // namespace rin
