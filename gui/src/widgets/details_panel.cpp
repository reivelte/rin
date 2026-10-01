// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <QtWidgets/QTableWidgetItem>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QStyle>
#include <QtWidgets/QHBoxLayout>
#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>
#include <QtGui/QTextOption>
#include <QtGui/QFontMetrics>
#include <QtGui/QColor>
#include <suzuri/utility/string.hpp>
#include "details_panel.hpp"
#include "model/entity.hpp"
#include "utility/sizing.hpp"
#include "utility/painting.hpp"
#include "utility/text.hpp"

namespace rin
{
    details_panel::details_panel(QWidget* parent, entity_model* model) :
        ui_panel(parent),
        m_items(), m_model(model),
        m_thumbnail(nullptr),
        m_details_splitter(nullptr),
        m_info_table(nullptr), m_tagview(nullptr),
        m_name_rect(), m_info_table_attr_name_col_width_ratio(0.35), m_padding(4)
    {
        if (!m_model)
        { throw std::runtime_error("model was nullptr"); }

        m_thumbnail = new ui_image(this, QPixmap());
        
        m_name_font = font();
        m_name_font.setBold(true);
        m_name_font.setPointSize(m_name_font.pointSize() + 2);
        
        m_info_table = new QTableWidget(this);
        m_info_table->viewport()->setBackgroundRole(QPalette::Window);
        m_info_table->setCornerButtonEnabled(false);
        m_info_table->setColumnCount(2);
        m_info_table->setShowGrid(false);
        m_info_table->horizontalHeader()->setStretchLastSection(true);
        m_info_table->horizontalHeader()->hide();
        m_info_table->verticalHeader()->hide();

        m_tagview = new tag_view(this, m_model, tag_view::viewmode::Block);

        m_details_splitter = new QSplitter(Qt::Orientation::Vertical, this);
        m_details_splitter->addWidget(m_info_table);
        m_details_splitter->addWidget(m_tagview);

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
            const auto& e = m_model->at(index);
            m_items.push_back(index);

            if (e.has_attribute(Icon))
            {
                // calling model->data() instead of e.attribute() allows us to get an entity thumbnail if one exists
                auto icon = qvariant_cast<QIcon>(m_model->data(index, Qt::ItemDataRole::DecorationRole));

                // TODO: request an appropriate thumbnail from thumb manager for the current size of this widget
                const auto s = rin::fit_under(size(), icon.actualSize(size(), QIcon::Mode::Normal, QIcon::State::On));
                m_set_thumbnail(icon.pixmap(s));
            }
            m_set_name(e.attribute<QString>(Name));
            m_set_attributes_in_table(e);

            const QString id = e.has_attribute(Id) ? e.attribute<QString>(Id) : m_model->id_for_index(index);
            const QString q = e.type() == sz::entity_type::File ? "!taglist:file://" + id : "!taglist:" + id;

            if (const QModelIndex node_index = m_model->query(q, false); m_model->valid_index(node_index))
            { m_watching.insert(node_index); }
        }
        m_adjust_widget_geometries();
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
        m_info_table->clear();
        update();
    }

    void details_panel::paintEvent(QPaintEvent* event)
    {
        QFrame::paintEvent(event);

        QPainter painter(this);
        m_name_layout.draw(&painter, m_name_rect.topLeft());
    }

    void details_panel::resizeEvent(QResizeEvent* event)
    {
        ui_panel::resizeEvent(event);

        m_recenter_thumbnail();
        m_adjust_widget_geometries();
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

    void details_panel::m_set_attributes_in_table(const reflexive_entity& e)
    {
        using enum entity_attribute_type;
        
        const auto& attrs = e.attribute_map();
        const int w = width();
        QString max_len_str;
        int row = 0;
        
        m_info_table->setRowCount(static_cast<int>(attrs.size()));
        
        for (const auto& [attr, value] : attrs)
        {
            switch (attr)
            {
            case Size:
            case Modified:
            case Created:
            case Accessed:
            case File_Type:
            {
                const QString attr_str = QString::fromStdString(sz::utility::to_string(attr) + ": ").replace("_", " ");
            
                if (attr_str.size() > max_len_str.size())
                { max_len_str = attr_str; }

                auto* attr_str_item = new QTableWidgetItem(attr_str);
                attr_str_item->setTextAlignment(Qt::AlignmentFlag::AlignTop | Qt::AlignmentFlag::AlignRight);

                auto* attr_value_item = new QTableWidgetItem(std::get<QString>(value));
                attr_value_item->setTextAlignment(Qt::AlignmentFlag::AlignTop);

                m_info_table->setItem(row, 0, attr_str_item);
                m_info_table->setItem(row, 1, attr_value_item);

                m_info_table->resizeRowToContents(row);
                m_info_table->setRowHeight(row, m_info_table->rowHeight(row) + m_padding);
                ++row;
                break;
            }
            
            default: continue;
            }
        }
        m_info_table->setRowCount(row);
        m_info_table->setColumnWidth(0, qCeil(static_cast<qreal>(w) * m_info_table_attr_name_col_width_ratio));
    }

    void details_panel::m_recenter_thumbnail()
    {
        auto thr = m_thumbnail->rect();
        thr.setX(thr.x() + (rect().center().x() - thr.center().x()));
        m_thumbnail->move(thr.topLeft());
    }

    void details_panel::m_set_thumbnail(const QPixmap& thumbnail)
    {
        m_thumbnail->set_image(thumbnail);
        m_recenter_thumbnail();
    }

    void details_panel::m_set_name(const QString& name)
    {
        m_name_layout.setText(name);
    }

    void details_panel::m_adjust_widget_geometries()
    {
        const int w = width();
        const int thumbnail_height = m_thumbnail->height();
        int used_height = thumbnail_height + m_padding * 2;
        
        const QString text = m_items.size() == 1 ? m_model->at(m_items[0]).attribute<QString>(Name) : "";

        m_name_layout.clearLayout();
        const QSize text_area = rin::layout_text(m_name_layout, m_name_font, Qt::AlignmentFlag::AlignCenter, w - (m_padding * 2));

        m_name_rect.setTopLeft(QPoint(m_padding, used_height));
        m_name_rect.setSize(text_area);
        used_height += text_area.height() + m_padding;

        m_details_splitter->move(QPoint(0, used_height));
        m_details_splitter->resize(w, height() - used_height);

        m_info_table->resizeRowsToContents();

        for (int i = 0; i < m_info_table->rowCount(); ++i)
        { m_info_table->setRowHeight(i, m_info_table->rowHeight(i) + m_padding); }
    }

    // TODO
    void details_panel::refresh_items(const QModelIndex& first, const QModelIndex last, const QList<int>& roles)
    {
        Q_UNUSED(roles);
        Q_UNUSED(first);
        Q_UNUSED(last);
    }

} // namespace rin
