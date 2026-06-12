// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "common.hpp"
#include "tree.hpp"

namespace rin
{
    void sort_entities(entity_tree_node& node)
    {
        using enum entity_attribute_type;
        using enum sz::entity_type;
        
        assert(node.descriptor.sort_key && node.descriptor.sort_order);
        auto sort_key = *node.descriptor.sort_key;
        const bool ascending = *node.descriptor.sort_order == Qt::SortOrder::AscendingOrder;

        auto cmp = [&](const QString& name_a, const QString& name_b) -> bool
        {
            const auto& a = node[name_a];
            const auto& b = node[name_b];
            const bool a_is_dir = a.type() == File && a.attribute<QFileInfo>(File_Info).isDir();
            const bool b_is_dir = b.type() == File && b.attribute<QFileInfo>(File_Info).isDir();

            if (a_is_dir && !b_is_dir)
            { return true; }
            else if (!a_is_dir && b_is_dir)
            { return false; }

            switch (sort_key)
            {
            case Name:
            {
                if (ascending)
                { return QString::localeAwareCompare(name_a, name_b) > -1; }
                return QString::localeAwareCompare(name_a, name_b) == -1;
            }

            // TODO
            default:
                qDebug() << "rin::sort_entities: not implemented";
                break;
            }
            return false;
        };
        std::sort(node.begin(), node.end(), cmp);

    }

} // namespace rin
