// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <suzuri/utility/visitor.hpp>
#include "descriptor.hpp"

namespace rin
{
    query_descriptor::query_descriptor()
    {
        set_uri("");
        set_size(0);
    }

    query_descriptor::query_descriptor(const QString& uri)
    {
        set_uri(uri);
    }

    void query_descriptor::adjust(const reflexive_entity& e, bool increment)
    {
        using enum entity_attribute_type;
        using enum sz::entity_type;
        
        const int adj = increment ? 1 : -1;
        u64* o = nullptr;
        
        if (e.type() == File)
        {
            if (const auto& fsinfo = e.attribute<QFileInfo>(File_Info); fsinfo.isDir()) {
                o = &dir_count;
            }
            else
            {
                o = &file_count;

                if (const auto s = static_cast<u64>(fsinfo.size()); increment) {
                    file_size += s;
                }
                else if (s <= file_size) { // otherwise this is an invalid adjust
                    file_size -= static_cast<u64>(fsinfo.size());
                }
            }
        }
        else if (e.type() == Tag || e.type() == Alias) {
            o = &tag_count;
        }
        else if (e.type() == Concept) {
            o = &concept_count;
        }

        if (o)
        {
            if (!increment && *o == 0) {
                return;
            }
            (*o) += adj;
        }
    }

    #define visitor_func(X) \
        sz::utility::basic_visitor visitor{ \
            [&](query_descriptor& d) -> void     X, \
            [&](index_descriptor& d) -> void     X, \
            [&](concept_descriptor& d) -> void     X, \
            [&](thumbnail_descriptor& d) -> void X \
        }; \
        std::visit(visitor, *const_cast<reflexive_descriptor*>(this));

    reflexive_descriptor& reflexive_descriptor::operator+=(const reflexive_entity& e)
    {
        visitor_func({
            d += e;
        })
        return *this;
    }

    reflexive_descriptor& reflexive_descriptor::operator-=(const reflexive_entity& e)
    {
        visitor_func({
            d -= e;
        })
        return *this;
    }

    void reflexive_descriptor::set_uri(QString uri)
    {
        visitor_func({
            d.set_uri(uri);
        })
    }

    void reflexive_descriptor::set_status(query_status s)
    {
        visitor_func({
            d.status = s;
        })
    }

    void reflexive_descriptor::set_sort_info(entity_attribute_type sort_key, Qt::SortOrder sort_order)
    {
        visitor_func({
            d.sort_info = std::make_tuple(sort_order, sort_key);
        })
    }

    void reflexive_descriptor::set_size(usize s)
    {
        visitor_func({
            d.set_size(s);
        })
    }

    void reflexive_descriptor::set_processed_count(usize count)
    {
        visitor_func({
            d.processed = count;
        })
    }

    void reflexive_descriptor::clear() noexcept
    {
        // TODO: do we also want to clear underlying values here?
        visitor_func({
            d.processed = 0;
            d.file_size = 0;
            d.dir_count = 0;
            d.file_count = 0;
            d.tag_count = 0;
            d.concept_count = 0;
            d.set_size(0);
        })
    }

    QString reflexive_descriptor::uri() const noexcept
    {
        QString u;
        visitor_func({
            u = d.uri();
        })
        return u;
    }

    QString reflexive_descriptor::text() const noexcept
    {
        QString u;
        visitor_func({
            u = d.text();
        })
        return u;
    }

    query_source reflexive_descriptor::source() const noexcept
    {
        query_source s;
        visitor_func({
            s = d.source;
        })
        return s;
    }

    sz::entity_type reflexive_descriptor::uri_type() const noexcept
    {
        sz::entity_type t;
        visitor_func({
            t = d.uri_type();
        })
        return t;
    }

    query_status reflexive_descriptor::status() const noexcept
    {
        query_status s;
        visitor_func({
            s = d.status;
        })
        return s;
    }

    std::optional<std::tuple<Qt::SortOrder, entity_attribute_type>> reflexive_descriptor::sort_info() const noexcept
    {
        std::optional<std::tuple<Qt::SortOrder, entity_attribute_type>> info;
        visitor_func({
            info = d.sort_info;
        })
        return info;
    }

    usize reflexive_descriptor::size() const noexcept
    {
        usize s = 0;
        visitor_func({
            s = d.size();
        })
        return s;
    }

    usize reflexive_descriptor::processed_count() const noexcept
    {
        usize c = 0;
        visitor_func({
            c = d.processed;
        })
        return c;
    }

} // namespace rin
