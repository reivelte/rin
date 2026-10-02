// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <QtWidgets/QSplitter>
#include <QtWidgets/QTableWidget>
#include <QtGui/QPixmap>
#include <QtGui/QIcon>
#include <QtGui/QTextLayout>
#include "panel.hpp"
#include "model/entity.hpp"
#include "model/entitymodel.hpp"
#include "widgets/tag_view/tag_view.hpp"
#include "widgets/image.hpp"

namespace rin
{
    class entity_metadata_view : public QFrame
    {
        Q_OBJECT

        public:
        entity_metadata_view(ui_panel* parent, entity_model* model, int padding);
        ~entity_metadata_view();

        void clear();
        void set_entity(const QModelIndex& index);
        void set_tags(const reflexive_entity& e);

        public:
        QSize sizeHint() const override;

        protected:
        void resizeEvent(QResizeEvent* event) override;

        protected slots:
        void insert_tags(const QModelIndex& node_index);

        private:
        inline void m_set_attributes_in_table(const reflexive_entity& e);
        inline void m_query_for_or_get_tags(const QModelIndex& index);
        inline void m_adjust_widget_geometries();

        private:
        using enum entity_attribute_type;
        QSet<QModelIndex> m_watching;
        entity_model* m_model;
        QTableWidget* m_info_table;
        tag_view* m_tagview;
        qreal m_info_table_attr_name_col_width_ratio;
        int m_padding;
    };

    /////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////

    class details_panel : public ui_panel
    {
        Q_OBJECT

        public:
        details_panel(QWidget* parent, entity_model* model);
        ~details_panel();

        void clear();
        void set_item(const QModelIndex& index);
        void set_items(const QList<QModelIndex>& indexes);

        // reimplemented protected functions
        protected:
        void paintEvent(QPaintEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        
        protected slots:
        void refresh_items(const QModelIndex& first, const QModelIndex last, const QList<int>& roles);

        private:
        void m_recenter_thumbnail();
        void m_set_thumbnail(const QPixmap& thumbnail);
        void m_set_name(const QString& name);
        void m_adjust_widget_geometries();

        private:
        using enum entity_attribute_type;
        QList<QModelIndex> m_items;
        
        entity_model* m_model;
        ui_image* m_thumbnail;
        entity_metadata_view* m_view;
        
        QTextLayout m_name_layout;
        QFont m_name_font;
        QRect m_name_rect;
        int m_padding;
    };
    
} // namespace rin
