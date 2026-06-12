// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <QtWidgets/QStyle>
#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>
#include <QtGui/QTextOption>
#include "tag_listing.hpp"

namespace rin
{
    tag_listing::tag_listing(QWidget* parent) :
        QFrame(parent), m_xpad(10), m_ypad(10)
    {

    }

    tag_listing::~tag_listing()
    {
    }

    void tag_listing::clear()
    {
        m_tags.clear();
        m_layout.clear();
        update();
    }

    void tag_listing::set_tags(const std::vector<reflexive_entity> &tags)
    {
        m_tags = tags;
        m_layout_items();
        update();
    }

    void tag_listing::add_tag(const reflexive_entity& tag)
    {
        m_tags.push_back(tag);
        m_layout_items();
        update();
    }

    void tag_listing::add_tags(const std::vector<reflexive_entity>& tags)
    {
        m_tags.append_range(tags);
        m_layout_items();
        update();
    }

    const std::vector<reflexive_entity>& tag_listing::current_tags() const
    {
        return m_tags;
    }

    // TODO: visual indicator that a given tag already exists in the db
    void tag_listing::paintEvent(QPaintEvent* event)
    {
        QFrame::paintEvent(event);
        QPainter painter(this);
        QTextOption text_opt;
        text_opt.setWrapMode(QTextOption::WrapMode::WrapAnywhere);
        text_opt.setTextDirection(Qt::LayoutDirection::LeftToRight);
        text_opt.setAlignment(QStyle::visualAlignment(Qt::LayoutDirection::LeftToRight, Qt::AlignmentFlag::AlignVCenter | Qt::AlignmentFlag::AlignHCenter));

        int i = 0;
        for (const auto& tag : m_tags)
        {
            const auto text = tag.attribute<QString>(Name);
            const QRect text_rect = m_layout[i];
            painter.drawRect(text_rect);
            painter.drawText(text_rect, text, text_opt);
            ++i;
        }
    }

    void tag_listing::mouseReleaseEvent(QMouseEvent* event)
    {
        QFrame::mouseReleaseEvent(event);

        if (event->button() & Qt::MouseButton::LeftButton)
        {
            const QPoint mouse = event->position().toPoint();
            if (const int index = m_item_at(mouse); index > -1)
            {
                m_erase_item(index);
                m_layout_items();
                update();
            }
        }
    }

    // TODO: scrolling when amount of tags overflows available space on the widget
    void tag_listing::m_layout_items()
    {
        m_layout.clear();
        const auto r = rect();
        auto metrics = fontMetrics();
        QPoint pos(r.topLeft() + QPoint(m_xpad, m_ypad));
        for (const auto& tag : m_tags)
        {
            const auto text = tag.attribute<QString>(Name);
            m_layout.emplace_back(pos, QSize(metrics.horizontalAdvance(text) + m_xpad, metrics.height() + m_ypad / 2));
            const auto& text_rect = m_layout.back();
            pos.rx() += text_rect.width();
            if (pos.x() >= r.right())
            {
                pos.setX(m_xpad);
                pos.setY(pos.y() + metrics.height() + m_ypad);
            }
        }
    }

    void tag_listing::m_erase_item(int index)
    {
        m_tags.erase(m_tags.begin() + index);
    }

    int tag_listing::m_item_at(QPoint p) const
    {
        for (int i = 0; std::cmp_less(i, m_layout.size()); ++i)
        {
            if (m_layout[i].contains(p))
            { return i; }
        }
        return -1;
    }

} // namespace rin
