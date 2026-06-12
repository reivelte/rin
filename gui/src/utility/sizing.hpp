// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <QtCore/QSize>
#include <QtCore/QtMath>

namespace rin
{
    constexpr QSize fit_under(const QSize& target_size, const QSize& source_size, const qreal width_ratio = 1.0, const qreal height_ratio = 1.0)
    {
        if (target_size.isEmpty())
        { return target_size; }

        const QSizeF source = source_size.isEmpty() ? target_size.toSizeF() : source_size.toSizeF();
        const qreal aspect = source.width() / source.height();

        const qreal max_w = target_size.width() * width_ratio;
        const qreal max_h = target_size.height() * height_ratio;
        qreal w = max_w;
        qreal h = max_w / aspect;

        if (h > max_h)
        {
            w = aspect * max_h;
            h = max_h;
        }
        return QSizeF(w, h).toSize();
    }
    
    constexpr int approximate_height(const QString& text, int max_width, qreal gw, int gh, int spacing_y)
    {
        // std::ceil not constexpr for doubles, let qt convert it for us
        return qCeil(static_cast<qreal>(text.size()) * gw / static_cast<qreal>(max_width)) * gh + spacing_y;
    }
    
} // namespace rin
