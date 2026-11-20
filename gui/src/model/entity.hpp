// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <unordered_set>
#include <QtCore/QMimeDatabase>
#include "common.hpp"

namespace rin
{
    class entity_model;
    class entity_tree;
    class entity_tree_node;
    class entity_data_manager;

    typedef std::unordered_set<QString> tag_set;
    
    class reflexive_entity : public sz::entity
    {
        using base = sz::entity;
        using base::base;

        public:
        static reflexive_entity from_uri_or_id(QString x, sz::entity_database* db = nullptr, std::optional<QFileInfo> fsinfo = std::nullopt);

        inline reflexive_entity();
        inline reflexive_entity(const QString& name, sz::entity_type type, bool is_indexed = false, bool has_children = false);
        ~reflexive_entity() = default;

        inline bool operator==(const reflexive_entity& rhs) const;
        inline bool operator==(const QString& rhs) const;
        inline bool operator!=(const QString& rhs) const;

        protected:
        inline void set_key(int key);
        inline void set_parent_key(int parent);
        
        void fill_extended_attributes(sz::entity_database& db, const QMimeDatabase& mimedb, bool get_tag_set);

        inline int key() const;
        inline int parent_key() const;

        private:
        using enum entity_attribute_type;
        friend class entity_model;
        friend class entity_tree;
        friend class entity_tree_node;
        friend class entity_data_manager;

        int m_parent;
        int m_self;
    };

    inline reflexive_entity::reflexive_entity() :
        base()
    {
        set_attribute(Name, QString());
    }

    inline reflexive_entity::reflexive_entity(const QString& name, sz::entity_type type, bool is_indexed, bool has_children) :
        base()
    {
        erase_attribute(Name);
        set_attribute(Name, name);
        set_type(type);
        set_indexed(is_indexed);
        set_has_children(has_children);
    }

    inline bool reflexive_entity::operator==(const reflexive_entity& rhs) const
    {
        return (attribute<QString>(Name) == rhs.attribute<QString>(Name)) && (parent_key() == rhs.parent_key());
    }

    inline bool reflexive_entity::operator==(const QString& rhs) const
    {
        return attribute<QString>(Name) == rhs;
    }

    inline bool reflexive_entity::operator!=(const QString& rhs) const
    {
        return !(*this == rhs);
    }

    inline void reflexive_entity::set_key(int key)
    {
        m_self = key;
    }

    inline void reflexive_entity::set_parent_key(int parent)
    {
        m_parent = parent;
    }

    inline int reflexive_entity::key() const
    {
        return m_self;
    }

    inline int reflexive_entity::parent_key() const
    {
        return m_parent;
    }

} // namespace rin
