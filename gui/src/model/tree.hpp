// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <memory>
#include <vector>
#include <unordered_map>
#include <stack>
#include <tuple>
#include <QtCore/QString>
#include <suzuri/result.hpp>
#include "common.hpp"
#include "descriptor.hpp"
#include "entity.hpp"

namespace rin
{
    class entity_data_manager;
    class entity_tree;
    enum class entity_tree_node_state : int { Valid, Invalidated, Updating, Constructing, Pending_Removal };

    class entity_tree_node
    {
        #define return_if_locked if (locked) { return; }
        
        public:
        entity_tree_node() :
            locked(false),
            descriptor(query_descriptor()),
            m_state(entity_tree_node_state::Constructing),
            m_parent(0), m_key(0),
            m_index(-1)
        {
        }

        explicit entity_tree_node(const reflexive_descriptor& desc, data_node_key pk, data_node_key k, int idx) :
            locked(false),
            descriptor(desc),
            m_state(entity_tree_node_state::Constructing),
            m_parent(pk), m_key(k),
            m_index(idx)
        {
        }

        entity_tree_node(const entity_tree_node& rhs) :
            locked(false),
            descriptor(rhs.descriptor),
            m_items(rhs.m_items), m_positions(rhs.m_positions),
            m_state(rhs.m_state),
            m_parent(rhs.m_parent), m_key(rhs.m_key),
            m_index(rhs.m_index)
        {
        }

        const reflexive_entity& operator[](int index) const { return m_items.at(m_positions[index]); }
        const reflexive_entity& operator[](const QString& name) const { return m_items.at(name); }
        reflexive_entity& operator[](int index) { return m_items.at(m_positions[index]); }
        reflexive_entity& operator[](const QString& name) { return m_items.at(name); }

        inline operator const reflexive_descriptor&() noexcept { return descriptor; }
        
        auto begin() { return m_positions.begin(); }
        auto end() { return m_positions.end(); }
        auto begin() const { return m_positions.cbegin(); }
        auto end() const { return m_positions.cend(); }
        auto cbegin() const { return m_positions.cbegin(); }
        auto cend() const { return m_positions.cend(); }
        
        inline void set_state(entity_tree_node_state s) noexcept { return_if_locked; m_state = s; }
        inline void set_index(int idx) noexcept { return_if_locked; m_index = idx; }
        inline void set_parent(int p) noexcept { return_if_locked; m_parent = p; }
        inline void set_key(int k) noexcept { return_if_locked; m_key = k; }
        inline void set_sort_info(entity_attribute_type sort_key, Qt::SortOrder sort_order)
        {
            return_if_locked;
            descriptor.set_sort_info(sort_key, sort_order);
        }

        void reserve(size_t n)
        {
            return_if_locked;
            m_items.reserve(n);
            m_positions.reserve(n);
        }

        void insert(reflexive_entity&& e)
        {
            return_if_locked;
            e.set_parent_key(m_key);
            if (const auto& name = e.attribute<QString>(entity_attribute_type::Name); contains(name)) {
                m_items[name] = std::move(e);
            }
            else
            {
                descriptor += e;
                m_positions.emplace_back(name);
                m_items.emplace(name, std::move(e));
            }
        }

        void erase(const QString& name)
        {
            return_if_locked;
            if (contains(name))
            {
                auto e = m_items[name];
                descriptor -= e;
                m_items.erase(name);
                auto new_end = std::remove(m_positions.begin(), m_positions.end(), name);
                m_positions.erase(new_end, m_positions.end());
            }
        }

        void erase(int idx)
        {
            return_if_locked;
            if (idx > -1 && std::cmp_less(idx, m_positions.size())) {
                erase(m_positions[idx]);
            }
        }

        void clear()
        {
            return_if_locked;
            descriptor.clear();
            m_items.clear();
            m_positions.clear();
            m_state = entity_tree_node_state::Valid;
        }

        const reflexive_descriptor& const_descriptor() const noexcept { return descriptor; }
        entity_tree_node_state state() const noexcept { return m_state; }
        data_node_key parent_key() const noexcept { return m_parent; }
        data_node_key key() const noexcept { return m_key; }
        int index() const noexcept { return m_index; }

        int size() const { return static_cast<int>(m_items.size()); }
        bool contains(const QString& name) const { return m_items.contains(name); }
        bool contains(int i) const { return 0 <= i && i < size(); }
        
        // TODO
        QString id(const QString& name) const
        {
            if (const auto& item = m_items.at(name); item.type() == sz::entity_type::File)
            {
                return item.attribute<QFileInfo>(entity_attribute_type::File_Info).absoluteFilePath();
            }
            return name;
        }

        int index_of(const QString& name) const
        {
            int i = 0;
            for (const auto& n : m_positions)
            {
                if (n == name)
                { return i; }
                ++i;
            }
            return -1;
        }

        protected:
        friend class entity_data_manager;
        friend class entity_tree;
        std::atomic<bool> locked; // mutations not allowed before dataman can make a copy of this node for itself
        reflexive_descriptor descriptor;

        private:
        std::unordered_map<QString, reflexive_entity> m_items;
        std::vector<QString> m_positions;
        entity_tree_node_state m_state;
        data_node_key m_parent;
        data_node_key m_key;
        int m_index;
    };

    template <is_descriptor D> requires (!std::same_as<D, query_descriptor>)
    data_node make_data_node(const D& d)
    {
        return std::make_shared<entity_tree_node>(reflexive_descriptor(d), data_node_key(0), data_node_key(0), -1);
    }

    data_node make_data_node(const query_descriptor& qd, data_node_key parent, data_node_key key, int index);
    data_node make_data_node(const entity_tree_node& node);

    class entity_tree
    {
        public:
        inline entity_tree();
        ~entity_tree() = default;

        inline auto begin() const;
        inline auto end() const;

        inline entity_tree_node& operator[](const data_node_key key);
        inline entity_tree_node& operator[](const QString& query_text);
        inline const entity_tree_node& operator[](const data_node_key key) const;
        inline const entity_tree_node& operator[](const QString& query_text) const;

        data_node_key make_node(const query_descriptor& qd, data_node_key parent, int index);
        void assign_node(const data_node_key key, data_node n);
        bool insert(const data_node_key key, reflexive_entity&& e);
        void reseat(const data_node_key key, const int index, reflexive_entity&& e);
        entity_tree_node& node(const data_node_key key);
        const entity_tree_node& node(const data_node_key key) const;
        data_node data_node_for_key(const data_node_key key) const;
        
        template <typename Func> requires sz::is_indicating_function<Func, const entity_tree_node&, int>
        void traverse(const data_node_key root, Func callback) const;
        
        inline void clear();
        inline void erase(const data_node_key key);
        inline void erase(const data_node_key key, const QString& name);
        inline void erase(const data_node_key key, const int index);

        void apply_keymap(const data_node_key key);
        
        inline bool empty() const;
        inline bool contains(const data_node_key key) const;
        inline bool contains(const query_descriptor& qd) const;
        inline bool contains(const QString& uri) const;
        bool contains(const data_node_key key, const QString& name) const;
        entity_tree_keymap keymap() const;

        int index_of(const data_node_key key, const QString& name) const;
        QString id_of(const data_node_key key, const reflexive_entity& e) const;

        inline int key_for_descriptor(const query_descriptor& qd) const;
        inline int key_for_uri(const QString& uri) const;

        private:
        template <typename Func> requires sz::is_indicating_function<Func, const entity_tree_node&>
        void m_backtrack(const data_node_key key, Func callback) const;

        inline void m_refresh_entity(reflexive_entity& e);
        inline int m_create_key();
        inline const reflexive_entity& m_item_representing_node(const entity_tree_node& node) const;
        inline entity_tree_node& m_node_for_key(const data_node_key key);
        inline const entity_tree_node& m_node_for_key(const data_node_key key) const;
        inline QString m_node_uri_for_entity(const reflexive_entity& e);

        private:
        using enum entity_attribute_type;
        std::unordered_map<data_node_key, data_node> m_nodes;
        std::unordered_map<QString, data_node_key> m_keys;
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

    inline entity_tree_node& entity_tree::operator[](const data_node_key key)
    {
        return m_node_for_key(key);
    }

    inline entity_tree_node& entity_tree::operator[](const QString& query_text)
    {
        return m_node_for_key(key_for_uri(query_text));
    }

    inline const entity_tree_node& entity_tree::operator[](const data_node_key key) const
    {
        return m_node_for_key(key);
    }

    inline const entity_tree_node& entity_tree::operator[](const QString& query_text) const
    {
        return m_node_for_key(key_for_uri(query_text));
    }

    // assumes root is a valid key in the tree
    template <typename Func> requires sz::is_indicating_function<Func, const entity_tree_node&, int>
    inline void entity_tree::traverse(const data_node_key root, Func callback) const
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
    inline void entity_tree::m_backtrack(const data_node_key key, Func callback) const
    {
        int k = key;
        while (m_nodes.contains(k))
        {
            const auto& node = m_node_for_key(k);
            k = node.parent_key();

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
    void entity_tree::erase(const data_node_key key)
    {
        m_keys.erase(m_node_for_key(key).descriptor.uri());
        m_nodes.erase(key);
    }

    inline void entity_tree::erase(const data_node_key key, const QString& name)
    {
        m_node_for_key(key).erase(name);
    }

    inline void entity_tree::erase(const data_node_key key, const int index)
    {
        m_node_for_key(key).erase(index);
    }

    inline bool entity_tree::empty() const
    {
        return m_nodes.empty();
    }

    inline bool entity_tree::contains(const data_node_key key) const
    {
        return m_nodes.contains(key);
    }

    inline bool entity_tree::contains(const query_descriptor& qd) const
    {
        return m_keys.contains(qd.uri());
    }

    inline bool entity_tree::contains(const QString& uri) const
    {
        return m_keys.contains(uri);
    }

    inline data_node_key entity_tree::key_for_descriptor(const query_descriptor& qd) const
    {
        return m_keys.at(qd.uri());
    }

    inline data_node_key entity_tree::key_for_uri(const QString& uri) const
    {
        if (contains(uri))
        { return m_keys.at(uri); }
        
        return 0;
    }

    inline data_node_key entity_tree::m_create_key()
    {
        ++m_keys_issued;
        return data_node_key(m_keys_issued);
    }

    inline const reflexive_entity& entity_tree::m_item_representing_node(const entity_tree_node& node) const
    {
        return (*m_nodes.at(node.parent_key()))[node.index()];
    }

    inline entity_tree_node& entity_tree::m_node_for_key(const data_node_key key)
    {
        return *m_nodes.at(key);
    }

    inline const entity_tree_node& entity_tree::m_node_for_key(const data_node_key key) const
    {
        return *m_nodes.at(key);
    }

    inline QString entity_tree::m_node_uri_for_entity(const reflexive_entity& e)
    {
        const auto& node = m_node_for_key(e.parent_key());
        return node.descriptor.uri();
    }

} // namespace rin
