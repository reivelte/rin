// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <ranges>
#include <concepts>
#include <QtCore/QRect>
#include <QtGui/QPainter>
#include <QtGui/QFontMetrics>
#include "model/entity.hpp"

namespace rin
{
    // assumes left-to-right text layout
    template <typename R> requires (std::ranges::range<R> && std::same_as<std::ranges::range_value_t<R>, QString>)
    std::vector<std::tuple<QString, QRect>> layout_strings(const R& strings, QFontMetrics font, QPoint p, int x_max, int xpadding, int ypadding)
    {
        std::vector<std::tuple<QString, QRect>> ret;
        const int x_min = p.x();
        for (const QString& string : strings)
        {
            QRect text_rect(p, QSize(font.horizontalAdvance(string) + xpadding, font.height() + ypadding / 2));
            ret.emplace_back(string, text_rect);
            p.rx() += text_rect.width();
            if (p.x() >= x_max)
            {
                p.setX(x_min);
                p.ry() += font.height() + ypadding;
            }
        }
        return ret;
    }
} // namespace rin
