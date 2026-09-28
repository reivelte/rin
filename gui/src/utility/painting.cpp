// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <QtGui/QPen>
#include <QtGui/QPainterStateGuard>
#include <QtGui/QPainterPath>
#include "painting.hpp"

namespace rin
{
    void draw_rounded_rect(QPainter& p, const QRect& r, const QColor& c, qreal xr, qreal yr, int pen_width)
    {
        QPainterStateGuard g(&p);
        p.setRenderHint(QPainter::Antialiasing);
        
        QPainterPath path;
        path.addRoundedRect(r.toRectF(), xr, yr);

        QPen pen(c, pen_width);
        p.setPen(pen);
        p.drawPath(path);
    }

} // namespace rin
