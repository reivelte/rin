// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <vector> 
#include "view/view.hpp"
#include "model/entity.hpp"

namespace rin
{
    class entity_model;
    class tag_view : public entity_view
    {
        Q_OBJECT
        
        public:
        enum class viewmode : int { Null, Block, Oneline };

        tag_view(QWidget* parent, entity_model* model, tag_view::viewmode mode);
        ~tag_view();

        void clear();
        void append_tags(const std::vector<reflexive_entity>& tags);
        void default_populate();

        std::vector<reflexive_entity> tags() const;
        QString concept_url() const;

        public slots:
        void set_thumbnail_size(int px);
        void set_thumbnails_enabled(bool enable);
        void set_tags(const std::vector<reflexive_entity>& tags);
        void set_viewmode(tag_view::viewmode mode);

        private:
        inline QString m_get_view_id_as_concept_url();
        inline void m_default_populate();
        inline void m_setup_oneline_mode();
        inline void m_setup_block_mode();
        inline void m_ensure_correct_root();
        void m_set_viewmode(tag_view::viewmode mode);

        private:
        using enum entity_attribute_type;
        using enum tag_view::viewmode;

        QModelIndex m_index_for_view_concept;
        entity_model* m_model;
        tag_view::viewmode m_mode;
        QString m_view_url;
        int m_thumb_size;
        bool m_thumbnails_enabled;
    };
    
} // namespace rin
