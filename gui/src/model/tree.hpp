// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <vector>
#include <unordered_map>
#include <stack>
#include <tuple>
#include <QtCore/QString>
#include <suzuri/result.hpp>
#include "common.hpp"

namespace rin
{
    enum class entity_tree_node_state : int { Valid, Invalidated, Updating, Constructing, Pending_Removal };

    struct entity_tree_node
    {
        query_descriptor descriptor;
        std::unordered_map<QString, reflexive_entity> items;
        std::vector<QString> positions;
        entity_tree_node_state state;
        int index;
        int parent;
        int key;

        const reflexive_entity& operator[](int index) const { return items.at(positions[index]); }
        const reflexive_entity& operator[](const QString& name) const { return items.at(name); }
        reflexive_entity& operator[](int index) { return items.at(positions[index]); }
        reflexive_entity& operator[](const QString& name) { return items.at(name); }
        
        auto begin() { return positions.begin(); }
        auto end() { return positions.end(); }
        auto begin() const { return positions.cbegin(); }
        auto end() const { return positions.cend(); }
        auto cbegin() const { return positions.cbegin(); }
        auto cend() const { return positions.cend(); }
        
        void reserve(size_t n)
        {
            items.reserve(n);
            positions.reserve(n);
        }
        void insert(reflexive_entity&& e)
        {
            const auto name = e.attribute<QString>(entity_attribute_type::Name);
            e.set_parent_key(key);
            positions.emplace_back(name);
            items.emplace(name, std::move(e));
        }
        void erase(const QString& name)
        {
            items.erase(name);
            auto new_end = std::remove(positions.begin(), positions.end(), name);
            positions.erase(new_end, positions.end());
        }
        int size() const { return static_cast<int>(items.size()); }
        bool contains(const QString& name) const { return items.contains(name); }
        bool contains(int i) const { return 0 <= i && i < size(); }
        QString id(const QString& name) const
        {
            if (const auto& item = items.at(name); item.type() == sz::entity_type::File)
            {
                return item.attribute<QFileInfo>(entity_attribute_type::File_Info).absoluteFilePath();
            }
            return name;
        }
        int index_of(const QString& name) const
        {
            int i = 0;
            for (const auto& n : positions)
            {
                if (n == name)
                { return i; }
                ++i;
            }
            return -1;
        }
    };

    class entity_tree
    {
        public:
        inline entity_tree();
        ~entity_tree() = default;

        inline auto begin() const;
        inline auto end() const;

        inline entity_tree_node& operator[](const int key);
        inline entity_tree_node& operator[](const QString& query_text);
        inline const entity_tree_node& operator[](const int key) const;
        inline const entity_tree_node& operator[](const QString& query_text) const;

        int make_node(const query_descriptor& qd, int index, int parent);
        bool insert(int k, reflexive_entity&& e);
        void reseat(const int key, const int index, reflexive_entity&& e);
        entity_tree_node& node(int key);
        const entity_tree_node& node(int key) const;
        
        template <typename Func> requires sz::is_indicating_function<Func, const entity_tree_node&, int>
        void traverse(const int root, Func callback) const;
        
        inline void clear();
        inline void erase(const int key);
        inline void erase(const int key, const QString& name);
        inline void erase(const int key, const int index);
        
        inline bool empty() const;
        inline bool contains(const int key) const;
        inline bool contains(const query_descriptor& qd) const;
        inline bool contains(const QString& query_text) const;
        bool contains(const int key, const QString& name) const;
        std::unordered_map<int, int> keys() const; // key to index map

        int index_of(const int key, const QString& name) const;
        QString id_of(const int key, const reflexive_entity& e) const;

        inline int key_for_descriptor(const query_descriptor& qd) const;
        inline int key_for_querytext(const QString& text) const;

        private:
        template <typename Func> requires sz::is_indicating_function<Func, const entity_tree_node&>
        void m_backtrack(int key, Func callback) const;

        inline void m_refresh_entity(reflexive_entity& e);
        inline int m_create_key();
        inline const reflexive_entity& m_item_representing_node(const entity_tree_node& node) const;
        inline entity_tree_node& m_node_for_key(int key);
        inline const entity_tree_node& m_node_for_key(int key) const;
        inline QString m_full_query_text_for_entity(const reflexive_entity& e);

        private:
        using enum entity_attribute_type;
        std::unordered_map<int, entity_tree_node> m_nodes;
        std::unordered_map<QString, int> m_keys;
        int m_keys_issued;
    };

    inline entity_tree::entity_tree() :
        m_keys_issued(0)
    {
    }

    inline auto entity_tree::begin() const
    {
        return m_nodes.begin();
    }

    inline auto entity_tree::end() const
    {
        return m_nodes.end();
    }

    inline entity_tree_node& entity_tree::operator[](const int key)
    {
        return m_node_for_key(key);
    }

    inline entity_tree_node& entity_tree::operator[](const QString& query_text)
    {
        return m_nodes.at(key_for_querytext(query_text));
    }

    inline const entity_tree_node& entity_tree::operator[](const int key) const
    {
        return m_node_for_key(key);
    }

    inline const entity_tree_node& entity_tree::operator[](const QString& query_text) const
    {
        return m_nodes.at(key_for_querytext(query_text));
    }

    // assumes root is a valid key in the tree
    template <typename Func> requires sz::is_indicating_function<Func, const entity_tree_node&, int>
    inline void entity_tree::traverse(const int root, Func callback) const
    {
        std::stack<std::tuple<int, int, int>> s; // key, pos, depth
        for (s.push({root, 0, 0}); s.size(); )
        {
            auto& [key, pos, depth] = s.top();
            const auto& node = m_node_for_key(key);
            bool do_next = false;
            for ( ; std::cmp_less(pos, node.size()); )
            {
                if (const int item_key = node[pos++].key(); m_nodes.contains(item_key))
                {
                    s.push({item_key, 0, depth + 1});
                    do_next = true;
                    break;
                }
            }
            if (do_next)
            { continue; }
            if (callback(node, depth))
            { return; }
            s.pop();
        }
    }

    
    template <typename Func> requires sz::is_indicating_function<Func, const entity_tree_node&>
    inline void entity_tree::m_backtrack(int key, Func callback) const
    {
        while (m_nodes.contains(key))
        {
            const auto& node = m_nodes.at(key);
            key = node.parent;

            if (callback(node))
            { return; }
        }
    }

    inline void entity_tree::clear()
    {
        m_nodes.clear();
        m_keys.clear();
        m_keys_issued = 0;
    }

    // does not remove child nodes
    void entity_tree::erase(const int key)
    {
        m_keys.erase(m_nodes.at(key).descriptor.full_text());
        m_nodes.erase(key);
    }

    inline void entity_tree::erase(const int key, const QString& name)
    {
        auto& node = m_node_for_key(key);
        if (node.items.contains(name))
        {
            auto new_end = std::remove(node.positions.begin(), node.positions.end(), name);
            node.positions.erase(new_end, node.positions.end());
            node.items.erase(name);
        }
    }

    inline void entity_tree::erase(const int key, const int index)
    {
        if (std::cmp_less(index, m_node_for_key(key).items.size()))
        {
            // we take a copy of name here because positions will shuffle items around
            const QString name = m_nodes[key].positions[index];
            erase(key, name);
        }
    }

    inline bool entity_tree::empty() const
    {
        return m_nodes.empty();
    }

    inline bool entity_tree::contains(const int key) const
    {
        return m_nodes.contains(key);
    }

    inline bool entity_tree::contains(const query_descriptor& qd) const
    {
        return m_keys.contains(qd.full_text());
    }

    inline bool entity_tree::contains(const QString& query_text) const
    {
        return m_keys.contains(query_text);
    }

    inline int entity_tree::key_for_descriptor(const query_descriptor& qd) const
    {
        return m_keys.at(qd.full_text());
    }

    inline int entity_tree::key_for_querytext(const QString& text) const
    {
        return m_keys.at(text);
    }

    inline int entity_tree::m_create_key()
    {
        ++m_keys_issued;
        return m_keys_issued;
    }

    inline const reflexive_entity& entity_tree::m_item_representing_node(const entity_tree_node& node) const
    {
        return m_nodes.at(node.parent)[node.index];
    }

    inline entity_tree_node& entity_tree::m_node_for_key(int key)
    {
        return m_nodes.at(key);
    }

    inline const entity_tree_node& entity_tree::m_node_for_key(int key) const
    {
        return m_nodes.at(key);
    }

    inline QString entity_tree::m_full_query_text_for_entity(const reflexive_entity& e)
    {
        const auto& node = m_node_for_key(e.parent_key());
        return node.descriptor.full_text();
    }

} // namespace rin
