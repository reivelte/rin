// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include "../../utility/sizing.hpp"
#include "core.cc"

namespace rin
{
    class entity_view::icon_mode : public entity_view::impl
    {
        public:
        entity_view_item_array items;
        std::vector<entity_view_item_row> rows;
        binary_space_partition<int> bsp;
        binary_space_partition<int> bsp_rows;
        int item_min_spacing_x;
        bool item_sizes_initialized;

        icon_mode(entity_view* parent);
        
        /* requires implementation */
        void init() override;
        std::vector<std::tuple<QModelIndex, int>> intersecting_set(const QRect& r, bool do_layout = false) override;
        entity_view_layout_descriptor prepare_item_layout() override;
        bool do_item_layout(const entity_view_layout_descriptor& info = entity_view_layout_descriptor()) override;

        /* requires implementation - driving class utility functions */
        QSize item_size_for_model_index(const QModelIndex& index) const override;
        QRect rect_for_model_index(const QModelIndex& index) const override;

        /* base implementation provided */
        void clear() override;
        void scroll_contents_by(int dx, int dy, bool scroll_elastic_band) override;

        /* inline functions */
        inline int default_row_width(int num_cols, int spacing_x) const;
        inline std::tuple<int, int> minimum_row_x(const QRect& r, int num_cols) const; // also returns the spacing_x used to derive the minimum
        inline void init_trees(const QRect& vp, size_t n);
        inline int items_per_row() const;
        inline QSize approximate_content_size(size_t item_count) const;
        inline int row_count_for_item_count(int item_count, int column_count) const;

        std::set<int> intersecting_rows(const QRect& r);
        void do_layout(const entity_view_layout_descriptor& info);
        void do_row_layout();
        void do_lazy_layout(int row_start, int row_end);
    };

    entity_view::icon_mode::icon_mode(entity_view* parent)
    : entity_view::impl(parent), item_sizes_initialized(false)
    {
        init();
    }

    void entity_view::icon_mode::init()
    {
        current_viewmode = entity_view_mode::Icon;
        elasticband = QRect();
        mouse_pressed_position = QPoint();
        
        // decorationAlignment
        thumbnail_alignment = Qt::AlignHCenter;

        // decorationPosition
        thumbnail_position = QStyleOptionViewItem::Position::Top; // TODO: make configurable

        // decorationSize
        // the thumbnail should fit within the set max icon size while maintaining its aspect ratio
        item_max_thumbnail_size = QSize();
        set_iconsize(QSize(256, 256));
        item_min_spacing_x = 8;
        
        // displayAlignment
        text_alignment = Qt::AlignmentFlag::AlignHCenter | Qt::AlignmentFlag::AlignVCenter; // TODO: make configurable
        // textElideMode
        text_elide_mode = Qt::TextElideMode::ElideRight; // TODO in delegate

        row_spacing_y = 20;
        show_elasticband = true;
        active_column = 0;

        item_alignment_in_row = entity_view_item_visual_align::Center;
    }

    std::vector<std::tuple<QModelIndex, int>> entity_view::icon_mode::intersecting_set(const QRect& r, bool do_layout)
    {
        if (do_layout) 
        { view->executeDelayedItemsLayout(); }

        std::vector<std::tuple<QModelIndex, int>> indexes;
        for (int viewitem_index : items.intersecting_set(r, bsp))
        {
            indexes.emplace_back(model->index(viewitem_index, active_column, root_index), viewitem_index);
        }
        return indexes;
    }

    entity_view_layout_descriptor entity_view::icon_mode::prepare_item_layout()
    {
        if (rows.empty() || std::cmp_not_equal(items.column_count(), items_per_row()))
        {
            const QRect vp = view->viewport()->rect();
            // const int batch_size = layout_batch_size; // TODO: batch size should apply in do_row_layout()
            const int modelsize = model->rowCount(root_index);
            const int num_cols = std::max((vp.width() / (item_max_width + item_min_spacing_x)), 1);
            const int num_rows_of_items = qCeil(static_cast<qreal>(modelsize) / static_cast<qreal>(num_cols));

            items.set_column_count(num_cols);
            rows.clear();
            rows.resize(num_rows_of_items);
            items.resize(num_rows_of_items);
            bsp.clear();
            bsp_rows.clear();
            init_trees(vp, modelsize);

            // TODO: lots of duplicated code here
            total_content_height = row_spacing_y;
            int h = 0;
            if (!item_sizes_initialized)
            {
                for (int i = 0; i < modelsize; ++i)
                {
                    const int col = i % num_cols;
                    QSize s;
                    if (items[i].size().isNull())
                    {
                        s = item_size_from_model(root_index, active_column, i);
                        items[i].resize(s);
                        items[i].set_index_hint(i);
                        ++view_loaded_items;
                    }
                    else
                    { s = items[i].size(); }
                    
                    h = s.height() > h ? s.height() : h;
                    
                    if ((col == num_cols - 1) || (i == modelsize - 1))
                    {
                        total_content_height += (h + row_spacing_y);
                        rows[i / num_cols].height = h;
                        h = 0;
                    }
                }
                item_sizes_initialized = true;
            }
            else // a resize event occured
            {
                for (int i = 0; i < modelsize; ++i)
                {
                    const int col = i % num_cols;
                    const QSize s = items[i].size();
                    h = s.height() > h ? s.height() : h;
                    if ((col == num_cols - 1) || (i == modelsize - 1))
                    {
                        total_content_height += (h + row_spacing_y);
                        rows[i / num_cols].height = h;
                        h = 0;
                    }
                }
            }
            do_row_layout();
        } // if (rows.empty() || std::cmp_not_equal(items.column_count(), items_per_row()))
        
        return entity_view_layout_descriptor();
    }
    
    // TODO: switch to auto horizontal scrolling if max_item_width > r.width
    bool entity_view::icon_mode::do_item_layout(const entity_view_layout_descriptor& info)
    {
        Q_UNUSED(info);
        
        const std::set<int> intersecting = intersecting_rows(view->viewport()->rect().translated(offset()));
        if (intersecting.size())
        {
            const int last = *intersecting.rbegin();
            do_lazy_layout(*intersecting.begin(), last);
        }
        return true;
    }

    QSize entity_view::icon_mode::item_size_for_model_index(const QModelIndex& index) const
    {
        const int row = index.row();
        if (std::cmp_less(row, items.size()) && row > -1)
        {
            return items[index.row()].size();
        }
        return QSize();
    }

    QRect entity_view::icon_mode::rect_for_model_index(const QModelIndex& index) const
    {
        const int row = index.row();
        if (std::cmp_less(row, items.size()) && row > -1)
        {
            return items[index.row()].rect();
        }
        return QRect();
    }

    void entity_view::icon_mode::clear() 
    {
        impl::clear();
        rows.clear();
        items.clear();
        item_sizes_initialized = false;
        bsp.clear();
        bsp_rows.clear();
    }

    void entity_view::icon_mode::scroll_contents_by(int dx, int dy, bool scroll_elastic_band)
    {
        impl::scroll_contents_by(dx, dy, scroll_elastic_band);
        view->scheduleDelayedItemsLayout();
    }

    inline int entity_view::icon_mode::default_row_width(int num_cols, int spacing_x) const
    {
        return ((num_cols - 1) * ((item_max_width + item_min_spacing_x) + spacing_x)) + item_max_width;
    }

    inline std::tuple<int, int> entity_view::icon_mode::minimum_row_x(const QRect& r, int num_cols) const
    {
        const int spacing_x = num_cols > 1 ? (r.width() - (num_cols * (item_max_width + item_min_spacing_x))) / num_cols : 0;
        const int min_row_x = r.left() + (spacing_x / 2);
        return { min_row_x, spacing_x };
    }

    inline void entity_view::icon_mode::init_trees(const QRect& vp, size_t n)
    {
        bsp.create(n);
        bsp.init(vp | QRect(QPoint(), approximate_content_size(n)), bsp_node_type::X_Plane);
        bsp_rows.create(n);
        bsp_rows.init(vp | QRect(QPoint(), approximate_content_size(n)), bsp_node_type::X_Plane);
    }

    inline int entity_view::icon_mode::items_per_row() const
    {
        return view->viewport()->rect().width() / (item_max_width + item_min_spacing_x);
    }

    // this function assumes TopToBottom flow
    inline QSize entity_view::icon_mode::approximate_content_size(size_t item_count) const
    {
        const QSize viewsize = view->viewport()->rect().size();
        const int item_width_with_min_spacing = item_max_width + item_min_spacing_x;
        const int n = std::max(viewsize.width() / item_width_with_min_spacing, 1);
        const int num_rows = qCeil(static_cast<qreal>(item_count) / static_cast<qreal>(n));
        return QSize(viewsize.width(), (num_rows * (item_max_height + row_spacing_y)));
    }

    inline int entity_view::icon_mode::row_count_for_item_count(int item_count, int column_count) const
    {
        return qCeil(static_cast<qreal>(item_count) / static_cast<qreal>(column_count));
    }

    std::set<int> entity_view::icon_mode::intersecting_rows(const QRect& r)
    {
        std::set<int> indexes;
        bsp_rows.for_each_leaf(r, [&](const std::vector<int>& leaf, const QRect& rect) -> void 
        {
            for (int row_index : leaf)
            {
                if (rows[row_index].rect.intersects(rect))
                {
                    indexes.insert(row_index);
                }
            }
        });
        return indexes;
    }

    void entity_view::icon_mode::do_layout(const entity_view_layout_descriptor& info)
    {
        const int item_max_width_min_spaced = item_max_width + item_min_spacing_x;
        const int num_cols = static_cast<int>(items.column_count());
        const auto [min_row_x, spacing_x] = minimum_row_x(view->viewport()->rect(), num_cols);
        int model_index_row = info.viewitem_index_start;
        
        for (int i = info.viewitem_index_start; i < info.viewitem_index_end; ++i)
        {
            const int row_idx = i / num_cols;
            const int col_idx = i - (row_idx * num_cols);
            const int x = min_row_x + (item_max_width_min_spaced + spacing_x) * col_idx;
            const int y = rows[row_idx].rect.top(); // TODO, row alignment

            items[i].move(x, y);
            bsp.push(items[i].rect(), i);
            rows[row_idx].count += 1;
            ++model_index_row;
        }
    }

    void entity_view::icon_mode::do_row_layout()
    {
        const QRect r = view->viewport()->rect();
        const int num_cols = static_cast<int>(items.column_count());
        const int num_rows = row_count_for_item_count(model->rowCount(root_index), num_cols);
        const auto [min_row_x, spacing_x] = minimum_row_x(r, num_cols);
        const int min_row_y = r.top() + row_spacing_y;
        const int row_width = default_row_width(num_cols, spacing_x);
        int y = min_row_y;

        for (int i = 0; i < num_rows; ++i)
        {
            rows[i].rect = QRect(QPoint(min_row_x, y), QSize(row_width, rows[i].height));
            rows[i].index = static_cast<size_t>(i) * num_cols;
            rows[i].count = 0;
            rows[i].hidden = false;
            bsp_rows.push(rows[i].rect, i);
            y += rows[i].height + row_spacing_y;
        }
    }

    void entity_view::icon_mode::do_lazy_layout(int row_start, int row_end)
    {
        row_start = std::max(row_start - 5, 0);
        row_end = std::min(row_end + 6, static_cast<int>(rows.size()));

        if (row_end == 0)
        { return; }

        const int item_end = std::min(static_cast<int>(rows[row_end - 1].index + items.column_count()), model->rowCount(root_index));
        int item_start = static_cast<int>(rows[row_start].index);

        while (bsp.contains(item_start) && item_start < item_end)
        { ++item_start; }

        if (item_start == item_end)
        { return; } // nothing to do

        entity_view_layout_descriptor desc{
            .model_indexes{}, // the range is always the same as start,end. (every item has the same parent and is in the same column)
            .parent = root_index,
            .bounding_rect = QRect(),
            .position = 0,
            .y_hint = rows[static_cast<size_t>(item_start) / items.column_count()].rect.top(),
            .viewitem_index_start = item_start,
            .viewitem_index_end = item_end
        };
        do_layout(desc);
    }

} // namespace rin
