// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "tree.hpp"
#include <suzuri/utility/string.hpp>

namespace rin
{
    data_node_key entity_tree::make_node(const query_descriptor& qd, data_node_key parent, int index)
    {
        const QString uri = qd.uri();
        
        if (contains(uri)) {
            return m_keys[uri];
        }
        const auto key = m_create_key();

        m_nodes.emplace(key, rin::make_data_node(qd, parent, key, index));
        m_keys.emplace(uri, key);
        
        if (m_nodes.contains(parent) && index > -1)
        {
            if (auto& n = m_node_for_key(parent); index < n.size()) {
                n[index].set_key(key);
            }
        }
        return key;
    }

    // n must be a 'standard' entity_tree_node (has a query_descriptor set inside its reflexive_descriptor variant)
    void entity_tree::assign_node(const data_node_key key, data_node n)
    {
        if (contains(key) && n->key() == key)
        {
            assert(std::holds_alternative<query_descriptor>(n->const_descriptor()));
            m_nodes[key] = n;
        }
    }

    bool entity_tree::insert(const data_node_key key, reflexive_entity&& e)
    {
        if (contains(key))
        {
            auto& n = m_node_for_key(key);
            if (n.locked) {
                return false;
            }
            n.insert(std::move(e));
            return true;
        }
        return false;
    }

    // the model is expected to emit dataChanged for items that are reseated
    void entity_tree::reseat(const data_node_key key, const int index, reflexive_entity&& e)
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
                if (node.descriptor.source() != query_source::Filesystem)
                { return false; } // skip this node
                std::vector<std::string> tokens;
                m_backtrack(node.key(), [&](const entity_tree_node& x) -> bool
                {
                    const int pk = x.parent_key();
                    const QString name = pk ? m_node_for_key(pk)[x.index()].attribute<QString>(Name) : QDir(x.descriptor.text()).dirName();
                    if (name == old_name && x.key() == stop_key)
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
                changes.emplace_back(QString(node.descriptor.text()).replace(old_path_comp, new_path_comp), node.key());
                return false;
            });
            // note that dataChanged doesn't need to be emitted for renamed nodes
            // this is because there is no change to the actual items representing these nodes
            for (const auto& [new_query_text, node_key] : changes)
            {
                auto& n = m_node_for_key(node_key);
                m_keys.erase(n.descriptor.uri());
                
                // new_query_text -> new_uri
                const QString new_uri = QString::fromStdString(sz::entity_uri(new_query_text.toStdString(), n.descriptor.uri_type()));
                
                m_keys.emplace(new_uri, node_key);
                n.descriptor.set_uri(new_uri);
            }
        }
        e.set_parent_key(key);
        e.set_key(node[old_name].key());

        // should private members be directly accessed like this?
        node.m_items.erase(old_name);
        node.m_positions[index] = new_name;
        node.m_items.emplace(new_name, std::move(e));
    }

    entity_tree_node& entity_tree::node(const data_node_key key)
    {
        return m_node_for_key(key);
    }

    const entity_tree_node& entity_tree::node(const data_node_key key) const
    {
        return m_node_for_key(key);
    }

    data_node entity_tree::data_node_for_key(const data_node_key key) const
    {
        if (contains(key))
        {
            return m_nodes.at(key);
        }
        return nullptr;
    }

    void entity_tree::apply_keymap(const data_node_key key)
    {
        if (contains(key))
        {
            auto& n = m_node_for_key(key);
            query_descriptor& qd = n.descriptor; // all nodes inside the tree use descriptor<void> aka query_descriptor for their descriptors
            qd.keymap = keymap();
        }
    }

    bool entity_tree::contains(const data_node_key key, const QString& name) const
    {
        return m_nodes.contains(key) && m_node_for_key(key).contains(name);
    }

    entity_tree_keymap entity_tree::keymap() const
    {
        entity_tree_keymap ret;
        for (const auto& [k, n] : m_nodes)
        {
            ret.emplace(k, n->index());
        }
        return ret;
    }

    int entity_tree::index_of(const data_node_key key, const QString& name) const
    {
        
        if (const auto& node = m_node_for_key(key); node.contains(name))
        {
            for (int i = 0; std::cmp_less(i, node.size()); ++i)
            {
                if (node.m_positions[i] == name)
                { return i; }
            }
        }
        return -1;
    }

    QString entity_tree::id_of(const data_node_key key, const reflexive_entity& e) const
    {
        QString id;
        if (m_nodes.contains(key))
        {
            id = m_node_for_key(key).id(e.attribute<QString>(Name));
        }
        return id;
    }

    data_node make_data_node(const query_descriptor& qd, data_node_key parent, data_node_key key, int index)
    {
        return std::make_shared<entity_tree_node>(qd, parent, key, index);
    }

    data_node make_data_node(const entity_tree_node& node)
    {
        return std::make_shared<entity_tree_node>(node);
    }

} // namespace rin
