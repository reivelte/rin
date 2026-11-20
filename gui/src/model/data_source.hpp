// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QtCore/QDirListing>
#include <suzuri/sqlite/query.hpp>
#include <suzuri/utility/visitor.hpp>
#include "descriptor.hpp"

namespace rin
{
    namespace detail
    {
        /* ******************** 
        Begin definitions for data sources
        ******************** */
        typedef sz::sqlite::database_query database_source;
        
        struct filesystem_source
        {
            QDirListing dir;
            QDirListing::const_iterator it;
        };
        
        struct concept_source
        {
            QList<QUrl> urls{};
            int position{};
        };
        /* ******************** 
        End definitions for data sources
        ******************** */

        template <typename T>
        struct data_source_container {};

        /* ******************** 
        Begin definitions for data source containers
        
        These include defining how to advance the position in and how to retrieve
        data from sources. Template specializations are used to avoid the
        overhead of inheritance.
        ******************** */
        template <>
        struct data_source_container<database_source>
        {
            database_source src;
            
            data_source_container(database_source&& s) : src(std::move(s)) {}
            
            inline operator database_source&() { return src; }
            inline data_source_container<database_source>& operator++()
            {
                ++src;
                return *this;
            }

            inline bool has_data() const { return src.good(); }
            inline QString id_from_source() const { return QString::fromStdString(src.get<std::string>()); }
            inline usize size() const { return static_cast<usize>(src.size()); }
        };

        template <>
        struct data_source_container<filesystem_source>
        {
            filesystem_source src;
            mutable std::optional<usize> src_size;

            data_source_container(filesystem_source&& s) : src(std::move(s)) {}
            
            inline operator filesystem_source&() { return src; }
            inline data_source_container<filesystem_source>& operator++()
            {
                ++src.it;
                return *this;
            }

            inline bool has_data() const { return src.it != src.dir.end(); }
            inline QString id_from_source() const { return src.it->absoluteFilePath(); }
            inline usize size() const
            {
                if (src_size)
                { return *src_size; }

                usize s = 0;
                auto& src_ = const_cast<filesystem_source&>(src);
                while (has_data())
                {
                    ++s;
                    ++src_.it;
                }
                src_.it = src_.dir.begin();
                src_size = s;
                return s;
            }
        };

        template <>
        struct data_source_container<concept_source>
        {
            concept_source src;

            data_source_container(concept_source&& s) : src(std::move(s)) {}
            data_source_container(const concept_source& s) : src(s) {}
            data_source_container(concept_source& s) : src(s) {}
            
            inline operator concept_source&() { return src; }
            inline data_source_container<concept_source>& operator++()
            {
                ++src.position;
                return *this;
            }

            inline bool has_data() const { return std::cmp_less(src.position, src.urls.size()); }
            inline QString id_from_source() const
            {
                // TODO: other url types
                return src.urls[src.position].toLocalFile(); 
            }
            inline usize size() const { return static_cast<usize>(src.urls.size()); }
        };

        /* ******************** 
        End definitions for data source containers
        ******************** */

        template <typename T>
        concept is_data_source = 
            std::same_as<T, filesystem_source> ||
            std::same_as<T, database_source> ||
            std::same_as<T, concept_source>;

        typedef std::variant<
            data_source_container<filesystem_source>,
            data_source_container<database_source>,
            data_source_container<concept_source>
        > data_source_variant;

    } // namespace rin::detail

    class data_source : public detail::data_source_variant
    {
        using base = detail::data_source_variant;
        using base::base;
        using enum query_source;

        public:
        explicit data_source(detail::database_source&& src) :
            base(std::move(src)), m_source_type(Index)
        {
            // move the query object to the first valid row, if there is one
            auto& data = std::get<detail::data_source_container<detail::database_source>>(*this);
            ++data;
        }
        data_source(const detail::database_source&) = delete;
        data_source(detail::database_source&) = delete;
        
        explicit data_source(detail::filesystem_source&& src) :
            base(std::move(src)), m_source_type(Filesystem)
        {
            auto& data = std::get<detail::data_source_container<detail::filesystem_source>>(*this);
            data.src.it = data.src.dir.begin();
        }

        data_source(const detail::filesystem_source&) = delete;
        data_source(detail::filesystem_source&) = delete;

        data_source(const detail::concept_source& src) : base(src), m_source_type(Concept) { }
        data_source(detail::concept_source&& src) : base(std::move(src)), m_source_type(Concept) { }
        data_source(detail::concept_source& src) : base(src), m_source_type(Concept) { }

        operator detail::filesystem_source&() { return std::get<detail::data_source_container<detail::filesystem_source>>(*this).src; }
        operator detail::database_source&() { return std::get<detail::data_source_container<detail::database_source>>(*this).src; }
        operator detail::concept_source&() { return std::get<detail::data_source_container<detail::concept_source>>(*this).src; }
        operator detail::data_source_container<detail::filesystem_source>&() { return std::get<detail::data_source_container<detail::filesystem_source>>(*this); }
        operator detail::data_source_container<detail::database_source>&() { return std::get<detail::data_source_container<detail::database_source>>(*this); }
        operator detail::data_source_container<detail::concept_source>&() { return std::get<detail::data_source_container<detail::concept_source>>(*this); }
        
        constexpr query_source type() const noexcept { return m_source_type; }

        template <detail::is_data_source Source>
        inline bool has_data() const
        {
            return std::get<detail::data_source_container<Source>>(*this).has_data();
        }

        inline bool has_data() const
        {
            using namespace rin::detail;
            sz::utility::basic_visitor visitor{
                [&](const data_source_container<filesystem_source>& src) -> bool { return src.has_data(); },
                [&](const data_source_container<database_source>& src) -> bool { return src.has_data(); },
                [&](const data_source_container<concept_source>& src) -> bool { return src.has_data(); }
            };
            return std::visit(visitor, *this);
        }

        template <detail::is_data_source Source>
        inline QString id_from_source() const
        {
            return std::get<detail::data_source_container<Source>>(*this).id_from_source();
        }

        inline usize size() const
        {
            using namespace rin::detail;
            sz::utility::basic_visitor visitor{
                [&](const data_source_container<filesystem_source>& src) -> usize { return src.size(); },
                [&](const data_source_container<database_source>& src) -> usize { return src.size(); },
                [&](const data_source_container<concept_source>& src) -> usize { return src.size(); }
            };
            return std::visit(visitor, *this);
        }

        private:
        query_source m_source_type;
    };
    
} // namespace rin
