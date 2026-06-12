// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <QtCore/QRegularExpression>
#include <QtCore/QRegularExpressionMatch>
#include "iconsizer.hpp"

namespace rin
{
    icon_sizer::icon_sizer(QWidget* parent)
    : QSpinBox(parent)
    {
        setRange(32, 256);
        setSingleStep(16);
        setSuffix("px");
        setStepType(QAbstractSpinBox::StepType::DefaultStepType);
    }

    int icon_sizer::valueFromText(const QString& text) const
    {
        // https://doc.qt.io/qt-6/qspinbox.html
        static const QRegularExpression reg_exp(tr("(\\d+)(\\s*[xx]\\s*\\d+)?"));
        Q_ASSERT(reg_exp.isValid());

        const QRegularExpressionMatch match = reg_exp.match(text);
        if (match.isValid())
            return match.captured(1).toInt();
        return 0;
    }

    QString icon_sizer::textFromValue(int value) const
    {
        // https://doc.qt.io/qt-6/qspinbox.html
        return tr("%1 x %1").arg(value);
    }

} // namespace rin
