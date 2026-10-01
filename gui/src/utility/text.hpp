// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QtWidgets/QStyleOptionViewItem>
#include <QtGui/QTextLayout>

namespace rin
{
    // returns layout size
    QSize layout_text(QTextLayout& layout, const QFont& font, Qt::Alignment alignment, const int max_line_width, const int max_line_height = -1);
    
} // namespace rin
