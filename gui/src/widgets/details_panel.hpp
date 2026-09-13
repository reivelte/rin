// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
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
        using enum entity_attribute_type;
        QList<QModelIndex> m_items;
        QSet<QModelIndex> m_watching;
        entity_model* m_model;

        ui_image* m_thumbnail;
        tag_view* m_tagview;
    };
    
} // namespace rin
