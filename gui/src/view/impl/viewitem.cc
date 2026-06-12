// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <set>
#include <vector>
#include <QtCore/QDebug>
#include <QtCore/QPoint>
#include <QtCore/QRect>
#include "../../rin.hpp"
#include "utility/bsp.hpp"

namespace rin
{
    class entity_view_item
    {
        public:
        entity_view_item() : m_index_hint(-1), m_x(-1), m_y(-1), m_w(0), m_h(0) {}
        entity_view_item(int index_hint) : m_index_hint(index_hint), m_x(-1), m_y(-1), m_w(0), m_h(0) {}
        entity_view_item(const QRect& r, int index_hint) :
            m_index_hint(index_hint),
            m_x(r.left()), m_y(r.top()), 
            m_w(static_cast<int16_t>(r.width())), m_h(static_cast<int16_t>(r.height()))
        { }

        auto operator<=>(const entity_view_item& rhs) const = default;

        constexpr void set_index_hint(int i)    { m_index_hint = i; }
        
        constexpr void set_geometry(const QRect& r) { m_x = r.left(); m_y = r.top(); m_w = static_cast<int16_t>(r.width()); m_h = static_cast<int16_t>(r.height()); }
        constexpr void set_geometry(int x, int y, int w, int h) { m_x = x; m_y = y; m_w = static_cast<int16_t>(w); m_h = static_cast<int16_t>(h); }
        constexpr void move(const QPoint& p)        { m_x = p.x(); m_y = p.y(); }
        constexpr void move(int x, int y)           { m_x = x; m_y = y;}
        constexpr void resize(const QSize& s)       { m_w = static_cast<int16_t>(s.width()); m_h = static_cast<int16_t>(s.height()); }
        constexpr void set_width(int w)             { m_w = static_cast<int16_t>(w); }
        constexpr void set_height(int h)            { m_h = static_cast<int16_t>(h); }
        
        constexpr bool valid_geometry() const { return ( (m_x > -1) && (m_y > -1) && (m_w > 0) && (m_h > 0)); }
        constexpr bool can_paint() const      { return valid_geometry(); }
        constexpr int x() const               { return m_x; }
        constexpr int y() const               { return m_y; }
        constexpr int width() const           { return static_cast<int>(m_w); }
        constexpr int height() const          { return static_cast<int>(m_h); }
        constexpr QSize size() const          { return QSize(m_w, m_h); }
        constexpr QRect rect() const          { return QRect(m_x, m_y, m_w, m_h); }
        constexpr QPoint pos() const          { return QPoint(m_x, m_y); }
        constexpr int index_hint() const      { return m_index_hint; }

        private:
        int m_index_hint;
        int m_x, m_y;
        int16_t m_w, m_h;
        
    };

    struct entity_view_item_row
    {
        QRect rect{};
        size_t index{}; // into the entity_view_item_array
        size_t count{};
        int height{};
        bool hidden{};
    };

    class entity_view_item_array
    {
        public:
        entity_view_item_array();

        inline entity_view_item& operator[](size_t i)             { return m_items[i]; }
        inline const entity_view_item& operator[](size_t i) const { return m_items[i]; }
        inline entity_view_item& operator[](size_t row, size_t col) { return m_items[m_index_for_row_col(row, col)]; }
        inline const entity_view_item& operator[](size_t row, size_t col) const { return m_items[m_index_for_row_col(row, col)]; }

        constexpr auto begin() { return m_items.begin(); }
        constexpr auto begin() const { return m_items.begin(); }
        constexpr auto cbegin() const { return m_items.cbegin(); }
        constexpr auto end() { return m_items.end(); }
        constexpr auto end() const { return m_items.end(); }
        constexpr auto cend() const { return m_items.cend(); }

        inline void append(int index_hint);
        inline void append(const QRect& r, int index_hint);
        inline void insert(const QRect& r, int index_hint, size_t row, size_t col);
        inline void clear();
        inline void erase(std::vector<entity_view_item>::iterator first, std::vector<entity_view_item>::iterator last) { m_items.erase(first, last); }
        inline void reserve(size_t n);
        inline void resize(size_t num_rows);
        inline void set_column_count(size_t n);

        void adjust_row_to_items(size_t viewitem_index, entity_view_item_row& row);
        void move_row(size_t viewitem_index, const QPoint& p, entity_view_item_row& row);
        void row_valign(size_t viewitem_index, const entity_view_item_row& row);
        void auto_adjust(std::vector<entity_view_item_row>& rows, int spacing_y);
        void auto_adjust(std::vector<entity_view_item_row>& rows, size_t row_begin, size_t row_end, int spacing_y);
        
        constexpr bool empty() const noexcept { return m_items.empty(); }
        constexpr size_t column_count() const noexcept { return m_column_count; }
        constexpr size_t size() const noexcept { return m_items.size(); }
        constexpr size_t capacity() const noexcept { return m_items.capacity(); }
        constexpr int item_count() const noexcept { return static_cast<int>(m_items.size()); }
        inline size_t row_starting_index(size_t viewitem_index) const { return m_row_header_index(viewitem_index); }
        inline auto row_span(size_t viewitem_index) const { return m_row_span(viewitem_index); }
        std::vector<size_t> row_header_indexes() const;
        inline QRect row_rect(size_t viewitem_index) const;
        inline QSize row_size(size_t viewitem_index) const;
        inline QPoint row_pos(size_t viewitem_index) const;
        inline entity_view_item_row row_at(size_t index, bool index_is_row_id = true) const;
        std::set<int> intersecting_set(const QRect& r, const binary_space_partition<int>& tree) const;
        QRect bounding_rect() const;
        void print() const;

        private:
        inline size_t m_row_header_index(size_t i) const;
        inline size_t m_index_for_row_col(size_t row, size_t col) const;
        inline std::tuple<size_t, size_t> m_row_col_for_index(size_t index) const;
        inline size_t m_item_idx_to_row_id(size_t i) const;
        inline size_t m_row_id_to_item_idx(size_t id) const;

        template<typename ItType>
        constexpr std::tuple<ItType, size_t> m_construct_row_span(ItType begin, size_t i) const;
        constexpr std::span<const entity_view_item> m_row_span(size_t i) const;
        constexpr std::span<entity_view_item> m_row_span(size_t i);
        
        private:
        std::vector<entity_view_item> m_items;
        size_t m_column_count;
    };

    entity_view_item_array::entity_view_item_array()
    : m_items(), m_column_count(1)
    {
    }

    inline void entity_view_item_array::append(int index_hint)
    {
        m_items.emplace_back(index_hint);
    }

    inline void entity_view_item_array::append(const QRect& r, int index_hint)
    {
        m_items.emplace_back(r, index_hint);
    }

    inline void entity_view_item_array::insert(const QRect& r, int index_hint, size_t row, size_t col)
    {
        size_t idx = m_index_for_row_col(row, col);
        m_items[idx].set_geometry(r);
        m_items[idx].set_index_hint(index_hint);
    }

    inline void entity_view_item_array::clear()
    {
        m_items.clear();
    }

    inline void entity_view_item_array::reserve(size_t n)
    {
        m_items.reserve(n);
    }

    inline void entity_view_item_array::resize(size_t num_rows)
    {
        m_items.reserve(m_column_count * num_rows);
        m_items.resize(num_rows * m_column_count);
    }

    inline void entity_view_item_array::set_column_count(size_t n)
    {
        if (n > 0 && m_column_count != n)
        {
            m_column_count = n;
        }
    }

    void entity_view_item_array::adjust_row_to_items(size_t viewitem_index, entity_view_item_row& row)
    {
        if (m_items.empty())
        { return; }

        auto row_items = m_row_span(viewitem_index);
        int x = m_items[row.index].x();
        int y{};
        int w = m_items[row.index + row.count - 1].rect().right() - m_items[row.index].rect().left();
        int h{};
        for (size_t i = 0; i < row_items.size(); ++i)
        {
            const auto& item = row_items[i];
            y = (item.y() < y ? item.y() : y);
            h = (item.height() > h ? item.height() : h);
        }
        row.rect = QRect(x, y, w, h);
    }

    void entity_view_item_array::move_row(size_t viewitem_index, const QPoint& p, entity_view_item_row& row)
    {
        QRect& r = row.rect;   
        int dx = r.left() - p.x();
        int dy = r.top() - p.y();
        r.moveTopLeft(p);
        
        for (entity_view_item& item : m_row_span(viewitem_index))
        {
            item.move(QPoint(item.x() - dx, item.y() - dy));
        }
    }

    void entity_view_item_array::row_valign(size_t viewitem_index, const entity_view_item_row& row)
    {
        const QRect& r = row.rect;
        for (auto& item : m_row_span(viewitem_index))
        {
            QRect item_rect = item.rect();
            int x = item.x();
            item_rect.moveCenter(r.center());
            item_rect.moveLeft(x);
            item.set_geometry(item_rect);
        }
    }

    void entity_view_item_array::auto_adjust(std::vector<entity_view_item_row>& rows, int spacing_y)
    {
        auto_adjust(rows, 0, rows.size(), spacing_y);
    }

    // row_end is not included
    void entity_view_item_array::auto_adjust(std::vector<entity_view_item_row>& rows, size_t row_begin, size_t row_end, int spacing_y)
    {
        for (size_t i = row_begin; i + 1 < row_end; ++i)
        {
            if (rows[i].count && rows[i + 1].count)
            {
                const auto ur = rows[i].rect;
                const auto lr = rows[i + 1].rect;
                move_row(m_row_id_to_item_idx(i + 1), QPoint(lr.x(), ur.bottom() + spacing_y), rows[i + 1]);
            }
        }
    }

    std::vector<size_t> entity_view_item_array::row_header_indexes() const
    {
        std::vector<size_t> indexes;
        
        for (size_t i = 0; i < m_items.size(); i += m_column_count)
        {
            indexes.push_back(i);
        }

        return indexes;
    }

    inline QRect entity_view_item_array::row_rect(size_t viewitem_index) const
    {
        return QRect(row_pos(viewitem_index), row_size(viewitem_index));
    }

    inline QSize entity_view_item_array::row_size(size_t viewitem_index) const
    {
        auto row = m_row_span(viewitem_index);
        const int w = row[row.size() - 1].rect().right() - row[0].rect().left();
        int h;
        for (const auto& item : row)
        {
            h = (item.height() > h ? item.height() : h);
        }
        return QSize(w, h);
    }

    inline QPoint entity_view_item_array::row_pos(size_t viewitem_index) const
    {
        return m_row_span(viewitem_index)[0].pos();
    }

    inline entity_view_item_row entity_view_item_array::row_at(size_t index, bool index_is_row_id) const
    {
        const auto viewitem_index = (index_is_row_id ? m_row_id_to_item_idx(index) : index);
        const auto row_items = m_row_span(viewitem_index);
        const QRect r = row_rect(viewitem_index);
        return entity_view_item_row{
            .rect = r,
            .index = m_row_header_index(viewitem_index),
            .count = row_items.size(),
            .height = r.height(),
            .hidden = false
        };
    }

    std::set<int> entity_view_item_array::intersecting_set(const QRect& r, const binary_space_partition<int>& tree) const
    {
        std::set<int> items;
        tree.for_each_leaf(r, [&](const std::vector<int>& leaf, const QRect& rect)
        {
            for (int idx : leaf)
            {
                if (m_items[idx].rect().intersects(rect))
                { items.insert(idx); }
            } 
        });
        return items;
    }

    QRect entity_view_item_array::bounding_rect() const
    {
        QRect r;
        if (m_items.size())
        {
            r |= row_rect(0);
            r |= row_rect(m_items.size() - 1);
        }
        return r;
    }

    void entity_view_item_array::print() const
    {
        int row_id = 0;
        for (auto row_idx : row_header_indexes())
        {
            qDebug() << "Row[" << (row_id / m_column_count) << "] " << row_rect(row_idx) << " " << m_row_span(row_idx).size() << " item(s)";
            ++row_id;
        }
    }

    inline size_t entity_view_item_array::m_row_header_index(size_t viewitem_index) const
    {
        return viewitem_index - std::get<1>(m_row_col_for_index(viewitem_index));
    }

    inline size_t entity_view_item_array::m_index_for_row_col(size_t row, size_t col) const
    {
        return (row * m_column_count) + col;
    }

    // assumes m_column_count is never zero (see: set_column_count())
    inline std::tuple<size_t, size_t> entity_view_item_array::m_row_col_for_index(size_t viewitem_index) const
    {
        auto row = viewitem_index / m_column_count;
        auto col = viewitem_index % m_column_count;
        return { row, col };
    }

    inline size_t entity_view_item_array::m_item_idx_to_row_id(size_t viewitem_index) const
    {
        return std::get<0>(m_row_col_for_index(viewitem_index));
    }

    inline size_t entity_view_item_array::m_row_id_to_item_idx(size_t id) const
    {
        return m_column_count * id;
    }

    template <typename ItType>
    constexpr std::tuple<ItType, size_t> entity_view_item_array::m_construct_row_span(ItType begin, size_t i) const
    {
        i = m_row_header_index(i);
        size_t len = 0;
        for (size_t j = 0; j < m_column_count; ++j)
        {
            if (m_items[i + j].valid_geometry())
            { ++len; }
            else
            { break; } // rows cannot have gaps
        }
        return { begin + i, len };
    }

    constexpr std::span<const entity_view_item> entity_view_item_array::m_row_span(size_t i) const
    {
        auto [start, count] = m_construct_row_span(m_items.cbegin(), i);
        return { start, count };
    }

    constexpr std::span<entity_view_item> entity_view_item_array::m_row_span(size_t i)
    {
        auto [start, count] = m_construct_row_span(m_items.begin(), i);
        return { start, count };
    }

} // namespace rin