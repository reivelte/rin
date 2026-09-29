// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <QtWidgets/QSplitter>
#include <QtWidgets/QTableWidget>
#include <QtGui/QPixmap>
#include <QtGui/QIcon>
#include "panel.hpp"
#include "model/entity.hpp"
#include "model/entitymodel.hpp"
#include "widgets/tag_view/tag_view.hpp"
#include "widgets/image.hpp"

namespace rin
{
    class details_panel : public ui_panel
    {
        Q_OBJECT

        public:
        details_panel(QWidget* parent, entity_model* model);
        ~details_panel();

        void set_item(const QModelIndex& index);
        void set_items(const QList<QModelIndex>& indexes);

        void clear();

        // reimplemented protected functions
        protected:
        void paintEvent(QPaintEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        
        protected slots:
        void refresh_items(const QModelIndex& first, const QModelIndex last, const QList<int>& roles);
        void insert_tags(const QModelIndex& parent);

        private:
        void m_set_attributes_in_table(const reflexive_entity& e);
        void m_recenter_thumbnail();
        void m_set_thumbnail(const QPixmap& thumbnail);
        void m_adjust_widget_geometries();

        private:
        using enum entity_attribute_type;
        QList<QModelIndex> m_items;
        QSet<QModelIndex> m_watching;
        entity_model* m_model;
        ui_image* m_thumbnail;
        QSplitter* m_details_splitter;
        QTableWidget* m_info_table;
        tag_view* m_tagview;
        int m_padding;
    };
    
} // namespace rin
