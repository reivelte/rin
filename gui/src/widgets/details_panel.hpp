// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <optional>
#include <QtWidgets/QFrame>
#include <QtGui/QIcon>
#include "panel.hpp"
#include "model/entity.hpp"
#include "model/entitymodel.hpp"

namespace rin
{
    class details_panel : public ui_panel
    {
        Q_OBJECT

        public:
        details_panel(QWidget* parent, entity_model* model);
        ~details_panel();

        void set_item(const reflexive_entity& e);
        void set_items(const std::vector<reflexive_entity>& items);

        void clear();

        // reimplemented
        QSize sizeHint() const override;

        protected:
        void paintEvent(QPaintEvent* event);
        void mouseMoveEvent(QMouseEvent* event) override;

        private:
        QRect m_draw_tags(QPainter& painter, const reflexive_entity& e, const QPoint& p);

        private:
        entity_model* m_model;
        std::vector<reflexive_entity> m_items;
        int m_padding;
        int m_panel_width;

    };
    
} // namespace rin
