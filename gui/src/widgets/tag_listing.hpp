// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <vector>
#include <QtWidgets/QFrame>
#include "model/entity.hpp"

namespace rin
{
    class entity_model;
    class entity_view;
    class tag_listing : public QFrame
    {
        Q_OBJECT
        
        public:
        enum class viewmode : int { Null, Block, Oneline };

        tag_listing(QWidget* parent, entity_model* model, tag_listing::viewmode mode);
        ~tag_listing();

        void clear();
        void set_viewmode(tag_listing::viewmode mode);
        void set_tags(const std::vector<reflexive_entity>& tags);
        void add_tag(const reflexive_entity& tag);
        void add_tags(const std::vector<reflexive_entity>& tags);
        
        const std::vector<reflexive_entity>& current_tags() const;

        protected:
        void resizeEvent(QResizeEvent* event) override;
        void paintEvent(QPaintEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;

        private:
        void m_create_view(tag_listing::viewmode mode);
        void m_default_populate_view();
        void m_layout_items();
        void m_erase_item(int index);
        int m_item_at(QPoint p) const;

        private:
        using enum tag_listing::viewmode;
        using enum entity_attribute_type;
        
        entity_view* m_view;
        entity_model* m_model;
        std::vector<reflexive_entity> m_tags;
        std::vector<QRect> m_layout;
        tag_listing::viewmode m_mode;
        int m_xpad;
        int m_ypad;

    };
    
} // namespace rin
