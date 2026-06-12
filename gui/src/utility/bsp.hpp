// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <iostream>
#include <cassert>
#include <queue>
#include <set>
#include <unordered_set>
#include <vector>
#include <QtCore/QRect>
#include <QtCore/QPoint>
#include <QtCore/QSet>

// referenced from: qtbase.git/src/widgets/itemviews/qbsptree_p.h
namespace rin
{
    enum class bsp_node_type : int  { None = 0, Y_Plane, X_Plane, XY_Plane };

    template <typename T>
    class binary_space_partition
    {
        public:
        binary_space_partition() : m_depth(0) { }

        void create(size_t n, int d = -1);
        void init(const QRect& area, bsp_node_type type);
        std::set<int> leaf_indexes(const QRect& rect) const;
        
        template <typename Func> requires (std::invocable<Func, std::vector<T>&, QRect>)
        void for_each_leaf(const QRect& rect, Func callback);
        
        template <typename Func> requires (std::invocable<Func, const std::vector<T>&, QRect>)
        void for_each_leaf(const QRect& rect, Func callback) const;
        
        inline void clear();
        inline void push(const QRect& r, const T& arg);
        inline void remove(const QRect& r, const T& arg);
        inline size_t set_count() const                     { return m_set.size(); }
        inline size_t leaf_count() const                    { return m_leaves.size(); }
        inline const std::vector<int>& leaf(size_t i) const { return m_leaves.at(i); }
        inline bool can_insert() const                      { return leaf_count() > 0; }
        inline bool initialized() const                     { return m_initialized; }
        inline bool contains(const T& arg) const            { return m_set.contains(arg); }

        private:
        struct node
        {
            inline node() : pos(0), type(bsp_node_type::None) { }
            
            int pos;
            bsp_node_type type;
        };
            
        private:
        constexpr int m_first_child_index(int i) const { return (i * 2) + 1; }
        constexpr int m_parent_index(int i) const { return (i & 1) ? ((i - 1) / 2) : ((i - 2) / 2); }

        private:
        QSet<T> m_set;
        std::vector<std::vector<T>> m_leaves; // each leaf is intended to contain indices into the items list of entity_view
        std::vector<binary_space_partition<T>::node> m_nodes;
        int m_depth;
        bool m_initialized;
    };

    template <typename T>
    inline void rin::binary_space_partition<T>::create(size_t n, int d)
    {
        if (d == -1)
        {
            int c = 0;
            
            for ( ; n; ++c) 
            { n = n / 10; }

            m_depth = c << 1;
        }
        else 
        { m_depth = d; }
        
        m_depth = std::max(m_depth, 1);
        size_t s = (static_cast<uint64_t>(1) << m_depth) - 1;
        m_nodes.resize(s);
        m_leaves.resize(s + 1);
    }

    template <typename T>
    inline void binary_space_partition<T>::init(const QRect& area, bsp_node_type type)
    {
        assert(m_nodes.size());
        std::queue<std::tuple<QRect, int>> q;
        q.emplace(area, 0);
        for (int depth = m_depth; depth > 0; --depth)
        {
            using enum bsp_node_type;
            bsp_node_type t = None;
            if (type == XY_Plane) 
            {
                // 2D bsp
                t = (m_depth & 1) ? X_Plane : Y_Plane; 
            }
            else 
            { t = type; }
            
            const auto& [rect, index] = q.front();
            q.pop();
            
            const QPoint& center = rect.center();
            m_nodes[index].pos = (t == Y_Plane ? center.x() : center.y());
            m_nodes[index].type = t;

            QRect back = rect, front = rect;
            if (t == Y_Plane)
            {
                back.setRight(center.x() - 1);
                front.setLeft(center.x());
            }
            else // X_Plane
            {
                back.setBottom(center.y() - 1);
                front.setTop(center.y());
            }

            const int idx = m_first_child_index(index);
            q.emplace(back, idx);
            q.emplace(front, idx + 1);
        }
        m_initialized = true;
    }

    template <typename T>
    std::set<int> binary_space_partition<T>::leaf_indexes(const QRect& rect) const
    {
        using enum bsp_node_type;
        std::set<int> ret;
        std::queue<int> q;
        
        if (m_nodes.empty())
        { return ret; }
        
        for (q.push(0); q.size(); q.pop())
        {
            int index = q.front();
            if (std::cmp_greater_equal(index, m_nodes.size()))
            {
                ret.insert(index - static_cast<int>(m_nodes.size()));
                continue;
            }
            
            const auto [pos, type] = m_nodes[index];
            const int first_child_index = m_first_child_index(index);
            
            if (type == Y_Plane)
            {
                if (rect.left() < pos)      { q.push(first_child_index); }      // back
                if (rect.right() >= pos)    { q.push(first_child_index + 1); }  // front
            }
            else // X_Plane or XY_Plane 
            {
                if (rect.top() < pos)       { q.push(first_child_index); }      // back
                if (rect.bottom() >= pos)   { q.push(first_child_index + 1); }  // front
            }
        }
        return ret;
    }
    
    template <typename T>
    template <typename Func> requires (std::invocable<Func, std::vector<T>&, QRect>)
    inline void binary_space_partition<T>::for_each_leaf(const QRect& rect, Func callback)
    {
        for (int leaf_idx : leaf_indexes(rect))
        { callback(m_leaves[leaf_idx], rect); }
    }
    
    template <typename T>
    template <typename Func> requires (std::invocable<Func, const std::vector<T>&, QRect>)
    inline void binary_space_partition<T>::for_each_leaf(const QRect& rect, Func callback) const
    {
        for (int leaf_idx : leaf_indexes(rect))
        { callback(m_leaves[leaf_idx], rect); }
    }

    template <typename T>
    inline void binary_space_partition<T>::clear()
    {
        m_set.clear();
        m_leaves.clear();
        m_nodes.clear();
        m_initialized = false;
    }

    template <typename T>
    inline void binary_space_partition<T>::push(const QRect& r, const T& arg)
    {
        if (m_initialized)
        {
            for_each_leaf(r, [&](std::vector<T>& leaf, const QRect&)
            { 
                leaf.push_back(arg); 
            });
            m_set.insert(arg);
        }
    }

    template <typename T>
    inline void binary_space_partition<T>::remove(const QRect& r, const T& arg)
    {
        if (m_initialized)
        {
            for_each_leaf(r, [&](std::vector<T>& leaf, const QRect&) 
            { 
                std::erase(leaf, arg); 
            });
            m_set.remove(arg);
        }
    }
}
