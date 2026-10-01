#include "text.hpp"
// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

namespace rin
{
    QSize layout_text(QTextLayout& layout, const QFont& font, Qt::Alignment alignment, const int max_line_width, const int max_line_height)
    {
        QTextOption text_option;
        text_option.setWrapMode(QTextOption::WrapAnywhere);
        text_option.setTextDirection(Qt::LayoutDirection::LeftToRight);
        text_option.setAlignment(alignment);

        layout.setTextOption(text_option);
        layout.setFont(font);

        int last_visible_line = -1;
        qreal height = 0;
        qreal width_used = 0;
        int i = 0;
        layout.beginLayout();
        while (true) 
        {
            QTextLine line = layout.createLine();
            
            if (!line.isValid()) 
            { break; }

            line.setLineWidth(max_line_width);
            line.setPosition(QPointF(0, height));
            height += line.height();
            width_used = qMax(width_used, line.naturalTextWidth()); // always <= line_width
            
            // we assume that the height of the next line is the same as the current one
            if (max_line_height > 0 && last_visible_line && height + line.height() > max_line_height) 
            {
                const QTextLine next_line = layout.createLine();
                last_visible_line = next_line.isValid() ? i : -1;
                break;
            }
            ++i;
        }
        layout.endLayout();
        return QSize(qCeil(width_used), qCeil(height));
    }
    
} // namespace rin