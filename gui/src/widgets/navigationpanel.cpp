// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <QtCore/QRect>
#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>
#include <QtGui/QPainterStateGuard>
#include <QtWidgets/QStyle>
#include <QtSvg/QSvgRenderer>
#include "navigationpanel.hpp"

namespace rin
{
    navigation_panel::navigation_panel(QWidget* parent)
    : QFrame(parent), m_icon_size(32, 32), m_item_spacing(2), m_panel_width(128)
    {
        setMouseTracking(true);
    }

    void navigation_panel::clear()
    {
        m_targets.clear();
        m_items.clear();
        update();
    }

    void navigation_panel::add_target(const QFileInfo &target)
    {
        if (target.isDir())
        { m_targets.emplace_back(target); }
        m_layout_items();
        update();
    }

    void navigation_panel::add_targets(const std::vector<std::string>& paths)
    {
        for (const auto& path : paths)
        {
            if (QFileInfo info(QString::fromStdString(path)); info.isDir())
            { m_targets.push_back(info); }
        }
        m_layout_items();
        update();
    }

    void navigation_panel::set_icon_size(QSize size)
    {
        m_icon_size = size;
    }

    void navigation_panel::set_item_spacing(int spacing)
    {
        m_item_spacing = spacing;
    }

    navigation_panel_item navigation_panel::item_at(int index) const
    {
        if (index > -1 && std::cmp_less(index, m_items.size()))
        {
            return m_items[index];
        }
        return navigation_panel_item();
    }

    QSize navigation_panel::sizeHint() const
    {
        return QSize(m_panel_width, parentWidget()->height());
    }

    void navigation_panel::set_width(int w)
    {
        m_panel_width = w;
    }

    // lays out the items
    void navigation_panel::resizeEvent(QResizeEvent* e)
    {
        QFrame::resizeEvent(e);
        m_layout_items();
    }

    void navigation_panel::paintEvent(QPaintEvent* e)
    {
        QFrame::paintEvent(e);
        QPainter painter(this);
        for (int i = 0; std::cmp_less(i, m_items.size()); ++i)
        {
            QPainterStateGuard g(&painter);
            const auto& item = m_items[i];
            const QFileInfo& target = item.info;
            const QRect& item_rect = item.rect;

            QPoint p = item_rect.topLeft();
            
            QIcon ic = QIcon::fromTheme("folder"); // TODO: item mime type detection
            auto icpm = ic.pixmap(m_icon_size, QIcon::Normal, QIcon::On);
            painter.drawPixmap(QRect(p, m_icon_size), icpm);
            
            int x_adv = p.x() + m_icon_size.width() + m_item_spacing;
            QTextOption textopt;
            textopt.setWrapMode(QTextOption::WrapAnywhere);
            textopt.setTextDirection(Qt::LayoutDirection::LeftToRight);
            textopt.setAlignment(QStyle::visualAlignment(Qt::LayoutDirection::LeftToRight, Qt::AlignVCenter));
            painter.drawText(QRect(QPoint(x_adv, p.y()), QSize(item_rect.width() - x_adv, item_rect.height())), target.fileName(), textopt);

            // if hovered, draw the hovered rect
            if (item.hovered)
            {
                QBrush hover_highlight(QColor(255, 255, 255, 30));
                painter.fillRect(item_rect, hover_highlight);
            }
        }
    }

    void navigation_panel::mouseReleaseEvent(QMouseEvent* e)
    {
        int item_index = m_item_at(e->pos());
        if (item_index > -1)
        { emit item_clicked(item_index); }
    }

    void navigation_panel::mouseMoveEvent(QMouseEvent* e)
    {
        QRegion update_region;
        int item_index = m_item_at(e->position().toPoint());
        
        if (item_index > -1)
        { 
            m_items[item_index].hovered = true; 
            update_region |= m_items[item_index].rect;
        }
        
        // set all others to false
        for (int i = 0; std::cmp_less(i, m_items.size()); ++i)
        {
            if (i == item_index)
            { continue; }
            if (m_items[i].hovered)
            {
                update_region |= m_items[i].rect;
                m_items[i].hovered = false;
            }
        }

        update(update_region);
    }

    void navigation_panel::leaveEvent(QEvent* e)
    {
        QRegion update_region;
        for (auto& item : m_items)
        {
            if (item.hovered)
            {
                update_region |= item.rect;
                item.hovered = false;
            }
        }
        update(update_region);
        QWidget::leaveEvent(e);
    }

    void navigation_panel::m_layout_items()
    {
        m_items.clear();
        QRect panel = rect();
        int y = panel.top() + m_item_spacing;
        int i = 0;
        while ((y < panel.height()) && std::cmp_less(i, m_targets.size()))
        {
            QRect item_rect(QPoint(panel.left(), y), QSize(m_panel_width, m_icon_size.height()));
            y += item_rect.height() + m_item_spacing;
            m_items.emplace_back(navigation_panel_item{
                .info = m_targets[i],
                .rect = item_rect,
                .hovered = false
            });
            ++i;
        }
    }

    int navigation_panel::m_item_at(QPoint p) const
    {
        navigation_panel_item item;
        for (int i = 0; std::cmp_less(i, m_items.size()); ++i)
        {
            if (m_items[i].rect.contains(p))
            {
                // m_items.size() is always less than or equal to m_targets.size()
                return i;
            }
        }
        return -1;
    }

} // namespace rin
