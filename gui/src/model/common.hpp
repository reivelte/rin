// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <cstdint>
#include <optional>
#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtGui/QPixmap>
#include <suzuri/metadata/tags.hpp>
#include <suzuri/utility/thumbnail.hpp>
#include <suzuri/entity/entity.hpp>

namespace rin
{
    struct entity_tree_node;
    class reflexive_entity;

    using sz::entity_attribute_type;
    
    typedef int data_node_key;
    typedef std::unordered_map<data_node_key, int> entity_tree_keymap; // key to index map
    typedef std::shared_ptr<entity_tree_node> data_node;
    typedef std::tuple<QList<QModelIndex>, QList<QModelIndex>> data_node_layout_changes;
    typedef QList<std::tuple<QString, int>> data_node_index_removal_list;


    struct node_index_change
    {
        int node_key = 0;
        int original_index = 0;
        int new_index = 0;
    };

    void sort_entities(entity_tree_node& node);
    
} // namespace rin
