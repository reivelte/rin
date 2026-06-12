// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <QtWidgets/QStyle>
#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>
#include <QtGui/QTextOption>
#include <QtGui/QFontMetrics>
#include <QtGui/QColor>
#include "details_panel.hpp"
#include "utility/sizing.hpp"
#include "utility/painting.hpp"

namespace rin
{
    details_panel::details_panel(QWidget* parent, entity_model* model) :
        ui_panel(parent), m_model(model), m_items(), 
        m_padding(20), m_panel_width(512)
    {
    }

    details_panel::~details_panel()
    {
    }

    void details_panel::set_item(const reflexive_entity& e)
    {
        clear();
        m_items.push_back(e);
        update();
    }

    void details_panel::set_items(const std::vector<reflexive_entity>& items)
    {
        clear();
        m_items = items;
        update();
    }

    void details_panel::clear()
    {
        m_items.clear();
    }

    QSize details_panel::sizeHint() const
    {
        return QSize(m_panel_width, parentWidget()->height());
    }

    void details_panel::paintEvent(QPaintEvent* event)
    {
        using enum entity_attribute_type;
        QFrame::paintEvent(event);
        QPainter painter(this);
        const QRect r = rect();
        const QSize max_thumb_size = rin::fit_under(r.size(), QSize(512, 512));
        if (m_items.size() == 1)
        {
            const reflexive_entity& e = m_items[0];
            QRect thumb_rect(QPoint(0, 0), rin::fit_under(max_thumb_size, QSize(512, 512)));
            if (e.has_attribute(Icon))
            {
                const QIcon& icon = e.attribute<QIcon>();
                const QSize thumb_size = e.has_attribute(Sizehint) ? 
                    rin::fit_under(max_thumb_size, e.attribute<QSize>()) : 
                    icon.actualSize(max_thumb_size, QIcon::Mode::Normal, QIcon::State::On);
                
                thumb_rect.setSize(thumb_size);
                thumb_rect.moveCenter(r.center());
                thumb_rect.setY(r.top() + m_padding);

                qDebug() << thumb_size;
                if (const QPixmap thumb = m_model->thumbnail(e); !thumb.isNull())
                { painter.drawPixmap(thumb_rect, thumb); }
                else
                { painter.drawPixmap(thumb_rect, icon.pixmap(thumb_size, QIcon::Mode::Normal, QIcon::State::On)); }

            }
            else
            {
                // TODO
                // use a default icon
            }

            QTextOption text_opt;
            text_opt.setWrapMode(QTextOption::WrapMode::WrapAnywhere);
            text_opt.setTextDirection(Qt::LayoutDirection::LeftToRight);
            text_opt.setAlignment(QStyle::visualAlignment(Qt::LayoutDirection::LeftToRight, Qt::AlignmentFlag::AlignVCenter | Qt::AlignmentFlag::AlignHCenter));

            // entities always have a name attribute
            const auto name = e.attribute<QString>(Name);
            QRect text_rect(QPoint(0, thumb_rect.bottom() + m_padding), QSize(r.width(), 40));
            painter.drawText(text_rect, name, text_opt);

            m_draw_tags(painter, e, QPoint(r.left(), text_rect.bottom()));
        }
    }

    void details_panel::mouseMoveEvent(QMouseEvent* event)
    {
        ui_panel::mouseMoveEvent(event);
    }

    QRect details_panel::m_draw_tags(QPainter& painter, const reflexive_entity& e, const QPoint& p)
    {
        QTextOption text_opt;
        text_opt.setWrapMode(QTextOption::WrapMode::WrapAnywhere);
        text_opt.setTextDirection(Qt::LayoutDirection::LeftToRight);
        text_opt.setAlignment(QStyle::visualAlignment(Qt::LayoutDirection::LeftToRight, Qt::AlignmentFlag::AlignVCenter | Qt::AlignmentFlag::AlignHCenter));
        auto metrics = fontMetrics();

        QRect area;
        if (auto tags = e.attribute<tag_set>(); tags.size())
        {
            auto layout = rin::layout_strings(tags, metrics, p, rect().right(), 10, 10);
            for (const auto& [tag, tag_rect] : layout)
            {
                painter.drawRect(tag_rect);
                painter.drawText(tag_rect, tag, text_opt);
                area |= tag_rect;
            }
        }
        return area;
    }

} // namespace rin
