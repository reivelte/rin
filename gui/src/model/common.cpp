// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <suzuri/utility/string.hpp>
#include "common.hpp"
#include "tree.hpp"

namespace rin
{
    void sort_entities(entity_tree_node& node)
    {
        using enum entity_attribute_type;
        using enum sz::entity_type;

        static const std::unordered_map<entity_attribute_type, entity_attribute_type> attr_map{
            {Size, Size_Int},
            {Created, Created_Int},
            {Modified, Modified_Int}
        };
        
        assert(node.descriptor.sort_key && node.descriptor.sort_order);
        auto sort_key = *node.descriptor.sort_key;
        const bool ascending = *node.descriptor.sort_order == Qt::SortOrder::AscendingOrder;

        auto cmp = [&](const QString& name_a, const QString& name_b) -> bool // return true if a should occur before b
        {
            // ascending: smallest item at front of array/list
            const auto& a = node[name_a];
            const auto& b = node[name_b];
            const bool a_is_dir = a.type() == File && a.attribute<QFileInfo>(File_Info).isDir();
            const bool b_is_dir = b.type() == File && b.attribute<QFileInfo>(File_Info).isDir();
            const bool both_same_type = a.type() == b.type();

            if (a_is_dir && !b_is_dir)
            { return true; }
            else if (!a_is_dir && b_is_dir)
            { return false; }

            if ((both_same_type && a.type() == Tag) || (a.type() == Tag || b.type() == Tag))
            { sort_key = Name; }

            if (a.has_attribute(sort_key) && b.has_attribute(sort_key))
            {
                switch (sort_key)
                {
                case Name:
                case File_Type:
                {
                    const auto val_a = a.attribute<QString>(sort_key);
                    const auto val_b = b.attribute<QString>(sort_key);
                    return ascending ? QString::localeAwareCompare(val_a, val_b) < 0 : QString::localeAwareCompare(val_a, val_b) > 0;
                }
                case Size:
                case Created:
                case Modified:
                {
                    const auto k = attr_map.at(sort_key);
                    if (a.has_attribute(k) && b.has_attribute(k))
                    {
                        const qint64 val_a = a.attribute<qint64>(k);
                        const qint64 val_b = b.attribute<qint64>(k);
                        return ascending ? val_a < val_b : val_a > val_b;
                    }
                    break;
                }
                default:
                    qDebug() << "rin::sort_entities: sort by" << sz::utility::to_string(sort_key) << "not implemented";
                    break;
                }
            }
            return false;
        };
        std::sort(node.begin(), node.end(), cmp);
    }

} // namespace rin
