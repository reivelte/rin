// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <QtWidgets/QStylePainter>
#include <QtWidgets/QStyleOption>
#include <QtGui/QKeyEvent>
#include <QtGui/QPaintEvent>
#include "../utility/sizing.hpp"
#include "resizing_textedit.hpp"

namespace rin
{
    resizing_textedit::resizing_textedit(const QString& text, QWidget* parent, int max_size, bool limit_width)
    : QTextEdit(text, parent), m_max_size(max_size), m_limit_width(limit_width)
    {
        setAcceptRichText(false);
        setAutoFormatting(QTextEdit::AutoFormattingFlag::AutoNone);
        setUndoRedoEnabled(true);
        setAlignment(Qt::AlignCenter);
        setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
        if (!m_limit_width)
        {
            setLineWrapMode(QTextEdit::LineWrapMode::NoWrap);
        }

        connect(this, &QTextEdit::textChanged, this, &resizing_textedit::resize_to_contents);
    }

    void resizing_textedit::resize_to_contents()
    {
        if (m_limit_width)
        {
            resize(QSize(m_max_size, m_paragraph_height() + fontMetrics().height()));
        }
    }

    void resizing_textedit::keyPressEvent(QKeyEvent* e)
    {
        const int key = e->key();
        if ((key == Qt::Key_Enter) || (key == Qt::Key_Return))
        {
            emit editing_finished();
            return;
        }

        QTextEdit::keyPressEvent(e);
    }

    void resizing_textedit::paintEvent(QPaintEvent* e)
    {
        QTextEdit::paintEvent(e);
        QStylePainter p(viewport());
        const QFontMetrics font = fontMetrics();

        // TODO: remove hardcoded values
        const int w = width() - 4;
        const int h = (m_limit_width ? m_paragraph_height() : font.height()) + 4;

        p.setPen(palette().brush(QPalette::Normal, QPalette::Highlight).color());
        p.drawRect(QRect(QPoint(0, 0), QSize(w, h)));
    }

    int resizing_textedit::m_paragraph_height() const
    {
        const QFontMetrics font = fontMetrics();
        const int w = m_max_size;
        const int font_height = font.height();
        const int h = rin::approximate_height(
            toPlainText(),
            w,
            font.boundingRect("A").toRectF().width(),
            font_height,
            1
        );
        return h;
    }

} // namespace rin
