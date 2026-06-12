// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <vector>
#include <QtWidgets/QFrame>
#include "model/entity.hpp"

namespace rin
{
    class tag_listing : public QFrame
    {
        Q_OBJECT
        
        public:
        tag_listing(QWidget* parent);
        ~tag_listing();

        void clear();
        void set_tags(const std::vector<reflexive_entity>& tags);
        void add_tag(const reflexive_entity& tag);
        void add_tags(const std::vector<reflexive_entity>& tags);
        
        const std::vector<reflexive_entity>& current_tags() const;

        protected:
        void paintEvent(QPaintEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;

        private:
        void m_layout_items();
        void m_erase_item(int index);
        int m_item_at(QPoint p) const;

        private:
        using enum entity_attribute_type;
        std::vector<reflexive_entity> m_tags;
        std::vector<QRect> m_layout;
        
        int m_xpad;
        int m_ypad;

    };
    
} // namespace rin
