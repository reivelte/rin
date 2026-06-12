// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <QtGui/QResizeEvent>
#include <QtGui/QMouseEvent>
#include "panel.hpp"

namespace rin
{
    ui_panel::ui_panel(QWidget* parent) :
        QFrame(parent), m_panel_width(256)
    {
        using enum ui_panel::grip_edge_position;
        m_grip_edges[Left] = QSize();
        m_grip_edges[Right] = QSize();

        setMouseTracking(true);
    }

    ui_panel::~ui_panel()
    {
    }

    void ui_panel::set_panel_width(int w)
    {
        m_panel_width = w;
        resize(w, height());
    }

    void ui_panel::set_grip_edge_size(grip_edge_position position, int w)
    {
        m_grip_edges[position].setWidth(w);
        update();
    }

    QSize ui_panel::grip_size(grip_edge_position position) const
    {
        if (m_grip_edges.contains(position))
        { return m_grip_edges.at(position); }
        return QSize();
    }

    QSize ui_panel::sizeHint() const
    {
        if (QWidget* p = parentWidget(); p)
        { return QSize(m_panel_width, p->height()); }

        return QFrame::sizeHint();
    }

    void ui_panel::resizeEvent(QResizeEvent* event)
    {
        for (auto& [edge, size] : m_grip_edges)
        {
            size.setHeight(event->size().height());
        }
        QFrame::resizeEvent(event);
    }

    void ui_panel::mouseMoveEvent(QMouseEvent* event)
    {
        using enum grip_edge_position;
        const QRect r = rect();
        const QPoint p = event->position().toPoint();
        for (const auto& [edge, size] : m_grip_edges)
        {
            QRect edge_rect;
            switch (edge)
            {
            case Right: { edge_rect = QRect(r.right() - size.width(), 0, size.width(), size.height()); break; }
            case Left: { edge_rect = QRect(r.left(), 0, size.width(), size.height()); break; }
            default: break;
            }

            if (edge_rect.contains(p))
            {
                // do stuff
                // qDebug() << "ui_panel: mouse hovered over edge";
            }
        }
        QFrame::mouseMoveEvent(event);
    }

} // namespace rin
