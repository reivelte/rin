// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "tree.hpp"
#include <suzuri/utility/string.hpp>

namespace rin
{
    int entity_tree::make_node(const query_descriptor& qd, int index, int parent)
    {
        const QString ft = qd.full_text();
        if (contains(ft))
        { return m_keys[ft]; }

        const int key = m_create_key();
        m_nodes.emplace(key, entity_tree_node{
            .descriptor = qd, // to be filled in by datamanger
            .items{},
            .positions{},
            .state = entity_tree_node_state::Constructing,
            .index = index,
            .parent = parent,
            .key = key
        });
        m_keys.emplace(ft, key);
        
        if (m_nodes.contains(parent) && index > -1 && index < m_nodes[parent].size())
        { m_nodes[parent][index].set_key(key); }
        
        return key;
    }

    bool entity_tree::insert(int k, reflexive_entity&& e)
    {
        if (contains(k))
        {
            m_nodes[k].insert(std::move(e));
            return true;
        }
        return false;
    }

    // the model is expected to emit dataChanged for items that are reseated
    void entity_tree::reseat(const int key, const int index, reflexive_entity&& e)
    {
        auto& node = m_node_for_key(key);
        const auto& item = node[index];
        const QString old_name = item.attribute<QString>(Name);
        const QString new_name = e.attribute<QString>(Name);
        // FIXME: this section involves a lot of string copying
        if (m_nodes.contains(item.key()) && old_name != new_name)
        {
            // /mnt/x/tmp/test
            // /mnt/x/tmp/test/dank -> bank
            // needs query text update: /mnt/x/tmp/test/dank/foo
            std::vector<std::tuple<QString, int>> changes;
            const int stop_key = item.key();
            traverse(stop_key, [&](const entity_tree_node& node, int depth) -> bool
            {
                Q_UNUSED(depth);
                if (node.descriptor.type != query_descriptor::query_type::Filesystem)
                { return false; } // skip this node
                std::vector<std::string> tokens;
                m_backtrack(node.key, [&](const entity_tree_node& x) -> bool
                {
                    const QString name = x.parent ? m_nodes.at(x.parent).positions[x.index] : QDir(x.descriptor.text).dirName();
                    if (name == old_name && x.key == stop_key)
                    {
                        tokens.emplace_back(new_name.toStdString());
                        return true;
                    }
                    tokens.emplace_back(name.toStdString());
                    return false;
                });
                std::reverse(tokens.begin(), tokens.end());
                const QString new_path_comp = QString::fromStdString(sz::utility::join("/", tokens));
                tokens[0] = old_name.toStdString();
                const QString old_path_comp = QString::fromStdString(sz::utility::join("/", tokens));
                changes.emplace_back(QString(node.descriptor.text).replace(old_path_comp, new_path_comp), node.key);
                return false;
            });
            // note that dataChanged doesn't need to be emitted for renamed nodes
            // this is because there is no change to the actual items representing these nodes
            for (const auto& [new_query_text, node_key] : changes)
            {
                m_keys.erase(m_nodes[node_key].descriptor.full_text());
                m_keys.emplace(new_query_text, node_key);
                m_nodes[node_key].descriptor.text = new_query_text;
            }
        }
        e.set_parent_key(key);
        e.set_key(node.items[old_name].key());
        node.items.erase(old_name);
        node.positions[index] = new_name;
        node.items.emplace(new_name, std::move(e));
    }

    entity_tree_node& entity_tree::node(int key)
    {
        return m_node_for_key(key);
    }

    const entity_tree_node& entity_tree::node(int key) const
    {
        return m_node_for_key(key);
    }

    bool entity_tree::contains(const int key, const QString& name) const
    {
        if (m_nodes.contains(key))
        {
            return m_nodes.at(key).items.contains(name);
        }
        return false;
    }

    std::unordered_map<int, int> entity_tree::keys() const
    {
        std::unordered_map<int, int> ret;
        for (const auto& [k, n] : m_nodes)
        {
            Q_UNUSED(n);
            ret.emplace(k, n.index);
        }
        return ret;
    }

    int entity_tree::index_of(const int key, const QString& name) const
    {
        const auto& node = m_node_for_key(key);
        if (node.items.contains(name))
        {
            for (int i = 0; std::cmp_less(i, node.positions.size()); ++i)
            {
                if (node.positions[i] == name)
                { return i; }
            }
        }
        return -1;
    }

    QString entity_tree::id_of(const int key, const reflexive_entity& e) const
    {
        QString id;
        if (m_nodes.contains(key))
        {
            id = m_nodes.at(key).id(e.attribute<QString>(Name));
        }
        return id;
    }

} // namespace rin
