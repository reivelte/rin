// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <string>
#include "tag_view.hpp"
#include "model/entitymodel.hpp"

namespace rin
{
    tag_view::tag_view(QWidget* parent, entity_model* model, tag_view::viewmode mode) :
        entity_view(parent), m_model(model), m_mode(tag_view::viewmode::Null),
        m_view_url(m_get_view_id_as_concept_url()),
        m_thumb_size(32), m_thumbnails_enabled(false)
    {
        setModel(m_model);
        m_set_viewmode(mode);

        // set the view up to have the appearance of an ordinary widget
        viewport()->setBackgroundRole(QPalette::Window);

        // get an empty concept for use with this view
            // FIXME: this causes a job interruption in dataman unnecessarily
            // ideally dataman doesn't need to be involved when the list of urls passed to query() is empty
        m_index_for_view_concept = m_model->query(m_view_url, QList<QUrl>());
        setRootIndex(m_index_for_view_concept);
    }

    tag_view::~tag_view()
    {
    }

    void tag_view::clear()
    {
        m_model->clear(m_index_for_view_concept);
    }

    void tag_view::append_tags(const std::vector<reflexive_entity>& tags)
    {
        reset();
        m_ensure_correct_root();
        m_model->append_data(m_index_for_view_concept, tags);
    }

    void tag_view::default_populate()
    {
        m_default_populate();
    }

    std::vector<reflexive_entity> tag_view::tags() const
    {
        std::vector<reflexive_entity> ret;
        const QModelIndex root = rootIndex();
        for (int i = 0; i < m_model->rowCount(root); ++i)
        {
            const auto& e = m_model->at(m_model->index(i, 0, root));
            
            if (e.type() == sz::entity_type::Tag)
            { ret.emplace_back(e); }
        }
        return ret;
    }

    QString tag_view::concept_url() const
    {
        return m_view_url;
    }

    // TODO: check that px is a factor of 32
    void tag_view::set_thumbnail_size(int px)
    {
        m_thumb_size = px;
        
        if (m_thumbnails_enabled)
        { set_iconsize(QSize(m_thumb_size, m_thumb_size)); }
    }

    void tag_view::set_thumbnails_enabled(bool enable)
    {
        if (m_thumbnails_enabled == enable)
        { return; }
        
        m_thumbnails_enabled = enable;
        m_set_viewmode(m_mode);
    }

    void tag_view::set_tags(const std::vector<reflexive_entity>& tags)
    {
        m_ensure_correct_root();
        m_model->clear(m_index_for_view_concept);
        m_model->append_data(m_index_for_view_concept, tags);
    }

    void tag_view::set_viewmode(tag_view::viewmode mode)
    {
        if (mode == m_mode)
        { return; }
        
        m_set_viewmode(mode);
    }

    inline QString tag_view::m_get_view_id_as_concept_url()
    {
        const auto s = std::format("concept://tagview_{}", reinterpret_cast<intptr_t>(this));
        return QString::fromStdString(s);
    }

    inline void tag_view::m_default_populate()
    {
        setRootIndex(m_model->query("!taglist:"));
    }

    inline void tag_view::m_setup_oneline_mode()
    {
        entity_view::set_viewmode(entity_view_mode::List);
        set_indent(0);
    }

    inline void tag_view::m_setup_block_mode()
    {
        entity_view::set_viewmode(entity_view_mode::Icon);
    }

    inline void tag_view::m_ensure_correct_root()
    {
        setRootIndex(m_index_for_view_concept);
    }

    void tag_view::m_set_viewmode(tag_view::viewmode mode)
    {
        switch (mode)
        {
        case Oneline: { m_setup_oneline_mode(); break; }
        case Block: { m_setup_block_mode(); break; }
        default: { return; }
        }

        m_mode = mode;

        if (m_thumbnails_enabled)
        { set_iconsize(QSize(m_thumb_size, m_thumb_size)); }
        else
        { set_iconsize(QSize(0, 0)); }
    }

} // namespace rin
