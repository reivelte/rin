// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <QtWidgets/QStyle>
#include <QtWidgets/QHBoxLayout>
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
        ui_panel(parent),
        m_items(), m_model(model),
        m_thumbnail(nullptr), m_tagview(nullptr)
    {
        if (!m_model)
        { throw std::runtime_error("model was nullptr"); }

        m_thumbnail = new ui_image(this, QPixmap());
        m_tagview = new tag_view(this, m_model, tag_view::viewmode::Block);

        connect(m_model, &entity_model::dataChanged, this, &details_panel::refresh_items);
        connect(m_model, &entity_model::rowsInserted, this, &details_panel::insert_tags);
        connect(m_model, qOverload<const QModelIndex&>(&entity_model::query_done), this, &details_panel::insert_tags);
    }

    details_panel::~details_panel()
    {
    }

    void details_panel::set_item(const QModelIndex& index)
    {
        clear();
        if (m_model->valid_index(index))
        {
            m_items.push_back(index);

            const auto& e = m_model->at(index);
            if (e.has_attribute(Icon))
            {
                // calling model->data() instead of e.attribute() allows us to get an entity thumbnail if one exists
                auto icon = qvariant_cast<QIcon>(m_model->data(index, Qt::ItemDataRole::DecorationRole));

                // TODO: request an appropriate thumbnail from thumb manager for the current size of this widget
                const auto s = rin::fit_under(size(), icon.actualSize(size(), QIcon::Mode::Normal, QIcon::State::On));
                m_thumbnail->set_image(icon.pixmap(s));
                
                QRect thr = m_thumbnail->rect();
                thr.setX(thr.x() + (rect().center().x() - thr.center().x()));
                
                m_thumbnail->move(thr.topLeft());
            }
            m_tagview->move(QPoint(0, m_thumbnail->height()));
            m_tagview->resize(width(), height() - m_thumbnail->height());

            QString id;
            if (e.has_attribute(Id))
            { id = e.attribute<QString>(Id); }
            else
            { id = m_model->id_for_index(index); }

            QString q;
            if (e.type() == sz::entity_type::File)
            { q = "!taglist:file://" + id; }
            else
            { q = "!taglist:" + id; }

            if (const QModelIndex node_index = m_model->query(q); m_model->valid_index(node_index))
            { m_watching.insert(node_index); }
        }
        update();
    }

    void details_panel::set_items(const QList<QModelIndex>& indexes)
    {
        clear();
        m_items = indexes;

        // TODO: thumbnails for selections of items

        update();
    }

    void details_panel::clear()
    {
        m_items.clear();
        m_watching.clear();
        m_tagview->clear();
        update();
    }

    void details_panel::paintEvent(QPaintEvent* event)
    {
        using enum entity_attribute_type;
        QFrame::paintEvent(event);
        // QPainter painter(this);

        // if (m_items.size() == 1)
        // {
        //     QTextOption text_opt;
        //     text_opt.setWrapMode(QTextOption::WrapMode::WrapAnywhere);
        //     text_opt.setTextDirection(Qt::LayoutDirection::LeftToRight);
        //     text_opt.setAlignment(QStyle::visualAlignment(Qt::LayoutDirection::LeftToRight, Qt::AlignmentFlag::AlignVCenter | Qt::AlignmentFlag::AlignHCenter));

        //     // entities always have a name attribute
        //     const auto name = e.attribute<QString>(Name);
        //     QRect text_rect(QPoint(0, thumb_rect.bottom() + m_padding), QSize(r.width(), 40));
        //     painter.drawText(text_rect, name, text_opt);
        // }
    }

    void details_panel::resizeEvent(QResizeEvent* event)
    {
        ui_panel::resizeEvent(event);

        const int w = width();
        m_thumbnail->resize(w, m_thumbnail->heightForWidth(w));
        m_tagview->move(QPoint(0, m_thumbnail->height()));
        m_tagview->resize(width(), height() - m_thumbnail->height());
    }

    void details_panel::mouseMoveEvent(QMouseEvent* event)
    {
        ui_panel::mouseMoveEvent(event);
    }

    void details_panel::insert_tags(const QModelIndex& parent)
    {
        if (m_watching.contains(parent))
        {
            m_tagview->clear();
            std::vector<reflexive_entity> tags;
            for (int i = 0; i < m_model->rowCount(parent); ++i)
            {
                const reflexive_entity& e = m_model->at(m_model->index(i, 0, parent));
                
                if (e.type() == sz::entity_type::Tag)
                { tags.emplace_back(e); }
            }
            m_tagview->append_tags(tags);
            
        }
    }

    void details_panel::refresh_items(const QModelIndex& first, const QModelIndex last, const QList<int>& roles)
    {
        Q_UNUSED(roles);
        if (first.isValid() && m_items.contains(first) && first == last)
        {
            set_item(first);
        }
        // TODO
    }

} // namespace rin
