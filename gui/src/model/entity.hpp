// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <array>
#include <concepts>
#include <variant>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <typeindex>
#include <typeinfo>
#include <QtCore/QString>
#include <QtCore/QSize>
#include <QtCore/QUrl>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QMimeType>
#include <QtGui/QIcon>
#include <suzuri/entity/database.hpp>
#include <suzuri/entity/common.hpp>

namespace rin
{
    enum class entity_attribute_type : int
    {
        // applies to all entities
        Null_Attribute = 0,
        Name = 1,               // string
        Id,                     // string
        Parent_Id,              // string
        Tags,                   // set<string>
        Comments, Description,  // string
        Color,                  // int
        Url,                    // QUrl

        // applies to filesystem based entities
        Size, Modified, Created, Accessed, File_Type, // string
        Rating,                                       // int
        Icon,                                         // QIcon
        File_Info,                                    // QFileInfo
        Mime_Type,                                    // QMimeType

        // applies to filesystem based image entities
        Sizehint,               // tuple<int, int> or similar

        // TODO: fields for specfic file types and entities
    };

    typedef std::unordered_set<QString> tag_set;
    typedef std::variant<
        tag_set,
        QString, QSize, QFileInfo, QUrl,
        QIcon, QMimeType,
        int
    >
    entity_attribute_base;

    template <typename T>
    concept is_valid_entity_attribute_type = requires
    {
        requires 
        (std::same_as<T, QString> || std::convertible_to<T, QString>) ||
        std::same_as<T, tag_set> ||
        std::same_as<T, QSize> ||
        std::same_as<T, QFileInfo> ||
        std::same_as<T, QUrl> ||
        std::same_as<T, QIcon> ||
        std::same_as<T, QMimeType> ||
        (std::same_as<T, int> || sz::is_int_compatible<T>);
    };

    namespace detail
    {
        using enum entity_attribute_type;
        static const std::unordered_map<std::type_index, entity_attribute_type> s_default_attrs{
            {typeid(QString), Name},
            {typeid(tag_set), Tags},
            {typeid(QSize), Sizehint},
            {typeid(QFileInfo), File_Info},
            {typeid(QUrl), Url},
            {typeid(QIcon), Icon},
            {typeid(QMimeType), Mime_Type}
        };
    }

    class entity_attribute : public entity_attribute_base
    {
        using base = entity_attribute_base;
        using base::base;

        public:
        explicit entity_attribute(const char* text) : base(QString(text)) {}
        explicit entity_attribute(const std::string& text) : base(QString::fromStdString(text)) {}
    };

    #define ENT_ATTR_TMPL(X) template <is_valid_entity_attribute_type X>
    
    class reflexive_entity
    {
        public:
        inline reflexive_entity();
        inline reflexive_entity(const QString& name, sz::entity_type type, bool is_indexed = false, bool has_children = false);
        ~reflexive_entity() = default;

        inline bool operator==(const QString& rhs) const;
        inline bool operator!=(const QString& rhs) const;

        inline void set_key(int key);
        inline void set_parent_key(int parent);
        inline void set_indexed(bool indexed);
        inline void set_has_children(bool has_children);

        ENT_ATTR_TMPL(T) void set_attribute(entity_attribute_type attr, const T& value);
        ENT_ATTR_TMPL(T) const T& attribute(entity_attribute_type attr = entity_attribute_type::Null_Attribute) const;
        ENT_ATTR_TMPL(T) T& attribute(entity_attribute_type attr = entity_attribute_type::Null_Attribute);

        inline int key() const;
        inline int parent_key() const;
        
        inline bool has_attribute(entity_attribute_type attribute) const;
        inline bool has_children() const;
        inline bool is_indexed() const;
        inline bool valid() const;
        inline sz::entity_type type() const noexcept;

        private:
        using enum entity_attribute_type;
        std::unordered_map<entity_attribute_type, entity_attribute> m_attributes;
        sz::entity_type m_type;
        int m_parent;
        int m_self;
        bool m_is_indexed;
        bool m_has_children;

    };

    inline reflexive_entity::reflexive_entity() :
        m_attributes(), 
        m_type(sz::entity_type::Invalid), m_parent(0), m_self(0),
        m_is_indexed(false), m_has_children(false)
    {
        set_attribute(Name, "");
    }

    inline reflexive_entity::reflexive_entity(const QString& name, sz::entity_type type, bool is_indexed, bool has_children) :
        m_attributes(),
        m_type(type), m_parent(0), m_self(0),
        m_is_indexed(is_indexed), m_has_children(has_children)
    {
        set_attribute(Name, name);
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

    inline void reflexive_entity::set_indexed(bool indexed)
    {
        m_is_indexed = indexed;
    }

    inline void reflexive_entity::set_has_children(bool has_children)
    {
        m_has_children = has_children;
    }

    ENT_ATTR_TMPL(T) inline void reflexive_entity::set_attribute(entity_attribute_type attr, const T& value)
    {
        if (attr == Null_Attribute)
        { return; }
        
        m_attributes.emplace(attr, value);
    }

    ENT_ATTR_TMPL(T) inline const T& reflexive_entity::attribute(entity_attribute_type attr) const
    {
        // FIXME: non-const attribute() uses operator[] on the map, which breaks const-correctness
        return std::as_const(const_cast<reflexive_entity*>(this)->attribute<T>(attr));
    }

    ENT_ATTR_TMPL(T) inline T& reflexive_entity::attribute(entity_attribute_type attr)
    {
        auto t = Null_Attribute;
        if (attr == Null_Attribute)
        {
            // get the default attribute based on the type id
            if (std::type_index idx(typeid(T)); detail::s_default_attrs.contains(idx))
            { t = detail::s_default_attrs.at(idx); }
            else
            { throw sz::exception(sz::result_code::Invalid_Argument); }
        }
        else
        { t = attr; }

        entity_attribute& a = m_attributes[t];
        assert(std::holds_alternative<T>(a));
        return std::get<T>(a);
    }

    inline int reflexive_entity::key() const
    {
        return m_self;
    }

    inline int reflexive_entity::parent_key() const
    {
        return m_parent;
    }

    inline bool reflexive_entity::has_attribute(entity_attribute_type attribute) const
    {
        return m_attributes.contains(attribute);
    }

    inline bool reflexive_entity::has_children() const
    {
        return m_has_children;
    }

    inline bool reflexive_entity::is_indexed() const
    {
        return m_is_indexed;
    }

    inline bool reflexive_entity::valid() const
    {
        return attribute<QString>(Name).size() && (m_type != sz::entity_type::Invalid);
    }

    inline sz::entity_type reflexive_entity::type() const noexcept
    {
        return m_type;
    }

} // namespace rin
