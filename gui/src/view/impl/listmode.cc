// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <map>
#include <stack>
#include <unordered_set>
#include <QtCore/QHash>
#include <QtWidgets/QHeaderView>
#include <QtGui/QPainter>
#include <suzuri/types.hpp>
#include "core.cc"

namespace rin
{
    struct list_mode_tree_node
    {
        enum class node_state : int { Collapsed, Expanded, Hidden };

        entity_view_item_array items{};
        QModelIndex parent{};
        node_state state = node_state::Collapsed;
        int depth{}; // depth level of the tree. used to derive the x_indent for items under this node
        bool pending_expand = false;

        int size() const { return static_cast<int>(items.size()); }
    };

    struct expanded_node
    {
        std::map<int, QModelIndex> children{};
        int total_count{};

        auto begin() const { return children.begin(); }
        auto end() const { return children.end(); }
    };

    namespace detail
    {
        template <typename T>
        struct nulltype
        {};

        template <typename T>
        concept maybe_qmodelindex = requires
        {
            requires std::same_as<T, nulltype<QModelIndex>> || std::same_as<T, QModelIndex>;
        };
    }

    class entity_view::list_mode : public entity_view::impl
    {
        public:
        binary_space_partition<QModelIndex> bsp;
        QHash<QModelIndex, list_mode_tree_node> nodes;
        QHash<QModelIndex, expanded_node> expanded_nodes;
        std::unordered_set<int> applied_global_positions;
        std::unordered_set<QString> autoexpand_nodes; // when model_is_entity_model
        std::array<QMetaObject::Connection, 5> header_conns;
        QMetaObject::Connection sortheader_conn;
        QHeaderView* header;
        int visible_items;
        int padding_y;
        int x_indent_scale;

        list_mode(entity_view* parent);
        ~list_mode();
        
        /* requires implementation */
        void init() override;
        std::vector<std::tuple<QModelIndex, int>> intersecting_set(const QRect& r, bool do_layout = false) override;
        entity_view_layout_descriptor prepare_item_layout() override;
        bool do_item_layout(const entity_view_layout_descriptor& info = entity_view_layout_descriptor()) override;

        /* requires implementation - driving class utility functions */
        QSize item_size_for_model_index(const QModelIndex& index) const override;
        QRect rect_for_model_index(const QModelIndex& index) const override;

        /* event handlers */
        void paint(QStylePainter& painter, QStyleOptionViewItem& opt, const QModelIndex& index) override;

        /* base implementation provided */
        void clear() override;
        void scroll_contents_by(int dx, int dy, bool scroll_elastic_band) override;
        
        /* for QHeaderView and friends */
        void disconnect_all();
        void set_header(QHeaderView* h);
        void hide_column(int col);
        void show_column(int col);
        void resize_column_to_contents(int col);

        /* ... */
        bool try_create_node(const QModelIndex& index, const int depth);
        void create_node(const QModelIndex& index, list_mode_tree_node::node_state state, int depth);
        void expand_node(const QModelIndex& index);
        void autoexpand_child_nodes(const QModelIndex& index);
        void clear_node(const QModelIndex& index);
        void collapse_node(const QModelIndex& index);
        int content_height_for_node(const QModelIndex& index) const;
        void recalculate_content_height(); // used when icon size is changed
        std::tuple<int, int> intersecting_range(const QRect& r) const;
        
        /* templates */
        template <typename Func, typename T = detail::nulltype<QModelIndex>> requires
        detail::maybe_qmodelindex<T> && 
        (
            (sz::is_indicating_function<Func, const QModelIndex&, int, int, int> && std::same_as<T, detail::nulltype<QModelIndex>>) ||
            sz::is_indicating_function<Func, const QModelIndex&>
        )
        void traverse_tree(Func callback, const T& root = {}) const;

        template <typename Func> requires sz::is_indicating_function<Func, const QModelIndex&>
        void traverse_tree(Func callback, const QModelIndex& root, const bool visit_collapsed) const;

        template <typename Func> requires sz::is_indicating_function<Func, const QModelIndex&, entity_view_item&, int>
        void traverse_tree(Func callback, const QModelIndex& root, int offset, int gstart);

        template <typename Func> requires sz::is_indicating_function<Func, const QModelIndex&>
        void backtrack(const QModelIndex& index, Func callback) const;

        /* inline functions */
        inline void clear_layout_data();
        inline std::tuple<QModelIndex, int> item_at_global_position(int position) const;
        inline QRect branch_rect_for_index(const QModelIndex& index);
        inline QRect rect_for_auxiliary_item(const QModelIndex& index) const;
        inline void draw_row(QPainter* painter, const QModelIndex& index);
        inline void draw_branch(QPainter* painter, const QStyleOptionViewItem& branch_opt);
        inline void create_bsp();
        inline list_mode_tree_node& node_for_model_index(const QModelIndex& index);
        inline entity_view_item_array& array_for_model_index(const QModelIndex& index);
        inline entity_view_item& viewitem_for_model_index(const QModelIndex& index);
        inline int y_hint(int global_position) const;
        inline int default_item_rect_width() const;
        inline int default_item_rect_height() const;
        inline QSize approximate_content_size(size_t item_count) const;
    };

    entity_view::list_mode::list_mode(entity_view* parent)
    : entity_view::impl(parent), header(nullptr), x_indent_scale(40)
    {
        init();
        set_header(new QHeaderView(Qt::Horizontal, view));
    }

    entity_view::list_mode::~list_mode()
    {
        disconnect_all();

        if (header)
        { delete header; }
    }

    void entity_view::list_mode::init()
    {
        visible_items = 0;
        current_viewmode = entity_view_mode::List;
        elasticband = QRect();
        mouse_pressed_position = QPoint();
        
        // decorationAlignment
        thumbnail_alignment = Qt::AlignLeft | Qt::AlignHCenter;

        // decorationPosition
        thumbnail_position = QStyleOptionViewItem::Position::Left; // TODO: make configurable

        // decorationSize
        // the thumbnail should fit within the set max icon size while maintaining its aspect ratio
        item_max_thumbnail_size = QSize();
        set_iconsize(QSize(32, 32));
        
        // displayAlignment
        text_alignment = Qt::AlignmentFlag::AlignVCenter | Qt::AlignmentFlag::AlignLeft; // TODO: make configurable
        // textElideMode
        text_elide_mode = Qt::TextElideMode::ElideRight; // TODO in delegate

        row_spacing_y = 0;
        show_elasticband = true;
        active_column = 0;

        padding_y = 2;
    }

    std::vector<std::tuple<QModelIndex, int>> entity_view::list_mode::intersecting_set(const QRect& r, bool do_layout)
    {
        if (do_layout) 
        { view->executeDelayedItemsLayout(); }

        QSet<QModelIndex> seen;
        std::vector<std::tuple<QModelIndex, int>> indexes;
        bsp.for_each_leaf(r, [&](const std::vector<QModelIndex>& leaf, const QRect& rect) -> void
        {
            for (const QModelIndex& idx : leaf)
            {
                if (seen.contains(idx))
                { continue; }

                assert(idx.isValid());
                if (const QModelIndex parent = idx.parent(); nodes.contains(parent))
                {
                    if (const entity_view_item& item = nodes[parent].items[idx.row()]; item.rect().intersects(rect))
                    { indexes.emplace_back(idx, item.index_hint()); }
                }
                seen.insert(idx);
            }
        });
        return indexes;
    }

    entity_view_layout_descriptor entity_view::list_mode::prepare_item_layout() 
    {
        if (nodes.empty())
        {
            create_node(root_index, list_mode_tree_node::node_state::Expanded, 0);
        }
        else if (nodes[root_index].state == list_mode_tree_node::node_state::Collapsed)
        {
            expand_node(root_index);
        }
        
        if (!bsp.initialized())
        { create_bsp(); }

        if (model_is_entity_model && autoexpand_nodes.size())
        {
            autoexpand_child_nodes(root_index);
        }

        const auto [start, end] = intersecting_range(bounds);
        return entity_view_layout_descriptor{
            .model_indexes{},
            .parent = root_index,
            .bounding_rect = QRect(),
            .position = 0,
            .y_hint = row_spacing_y,
            .viewitem_index_start = start, // we use global_position indexes instead of viewitem_indexes here
            .viewitem_index_end = end
        };
    }

    bool entity_view::list_mode::do_item_layout(const entity_view_layout_descriptor& info)
    {
        const int end = info.viewitem_index_end;
        int gpos = info.viewitem_index_start;
        
        while (applied_global_positions.contains(gpos) && gpos <= end)
        { ++gpos; }

        if (gpos >= end)
        { return true; }

        if (const auto [start_parent, lpos] = item_at_global_position(gpos); nodes.contains(start_parent))
        {
            const int x_start = header->sectionPosition(0);
            const int w = header->sectionSize(0);
            const int h = default_item_rect_height();
            traverse_tree([&](const QModelIndex& parent, entity_view_item& item, int item_loc) -> bool
            {
                const int indent = nodes[parent].depth * x_indent_scale + x_indent_scale;
                item.set_geometry(x_start + indent, y_hint(gpos), std::max(w - indent, 1), h);
                bsp.push(item.rect(), model->index(item_loc, 0, parent));
                applied_global_positions.emplace(gpos);
                return ++gpos > end;
            }, start_parent, lpos, gpos);
        }
        return gpos >= end;
    }

    QSize entity_view::list_mode::item_size_for_model_index(const QModelIndex& index) const
    {
        return rect_for_model_index(index).size();
    }

    QRect entity_view::list_mode::rect_for_model_index(const QModelIndex& index) const
    {
        if (index.isValid())
        {
            const QModelIndex parent = nodes.contains(index.parent()) ? index.parent() : QModelIndex(root_index);
            const auto& items = nodes[parent].items;

            if (std::cmp_less(index.row(), items.size()))
            {
                return items[index.row()].rect();
            }
        }
        return QRect();
    }

    void entity_view::list_mode::paint(QStylePainter& painter, QStyleOptionViewItem& opt, const QModelIndex& index)
    {
        const auto& items = nodes[index.parent()].items;
        const int pos = items[index.row()].y() / default_item_rect_height();

        opt.features.setFlag(QStyleOptionViewItem::Alternate, pos % 2 != 0);
        const QStyle::State prev = opt.state;
        const QRect prev_rect = opt.rect;
        
        opt.state &= ~QStyle::State_Selected;
        opt.rect.setWidth(view->viewport()->rect().width());
        opt.rect.setLeft(0);

        painter.drawPrimitive(QStyle::PE_PanelItemViewRow, opt);
        opt.state = prev;
        opt.rect = prev_rect;
        
        QStyleOptionViewItem branch_opt;
        view->initViewItemOption(&branch_opt);
        branch_opt.rect = map_to_viewport(branch_rect_for_index(index));

        const size_t items_under_parent = items.size();
        const bool has_siblings = items_under_parent && std::cmp_less(index.row(), items_under_parent - 1);
        const bool has_children = nodes.contains(index);
        const bool is_expanded = has_children && (nodes[index].state == list_mode_tree_node::node_state::Expanded);

        branch_opt.state = (QStyle::State_Item | QStyle::State_Enabled | QStyle::State_Active
            | (has_siblings ? QStyle::State_Sibling : QStyle::State_None)
            | (has_children ? QStyle::State_Children : QStyle::State_None)
            | (is_expanded ? QStyle::State_Open : QStyle::State_None)
        );

        painter.drawPrimitive(QStyle::PE_IndicatorBranch, branch_opt);
        draw_row(&painter, index);
        painter.setClipRect(QRect(
            QPoint(header->sectionPosition(0), opt.rect.top()),
            QSize(header->sectionSize(0), opt.rect.height())
        ));
    }

    // autoexpand_nodes is not cleared here
    void entity_view::list_mode::clear()
    {
        impl::clear();
        bsp.clear();
        nodes.clear();
        expanded_nodes.clear();
        applied_global_positions.clear();
    }

    void entity_view::list_mode::scroll_contents_by(int dx, int dy, bool scroll_elastic_band)
    {
        impl::scroll_contents_by(dx, dy, scroll_elastic_band);
        view->scheduleDelayedItemsLayout();
    }

    void entity_view::list_mode::disconnect_all()
    {
        for (const QMetaObject::Connection& connection : header_conns)
        { QObject::disconnect(connection); }

        QObject::disconnect(sortheader_conn);
    }

    void entity_view::list_mode::set_header(QHeaderView* h)
    {
        if (header == h || !h)
        { return; }

        if (header && header->parent() == view)
        { delete header; }

        header = h;
        header->setParent(view);
        header->setSectionsMovable(true);
        header->setStretchLastSection(true);
        header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        header->setMinimumSectionSize(item_max_thumbnail_size.width() * 2);
        header->setDefaultSectionSize(item_max_thumbnail_size.width() * 6);
        // header->setFirstSectionMovable(false);

        if (!header->model() && model)
        {
            header->setModel(model);
            header->setSelectionModel(view->selectionModel());
        }

        header_conns = {
            view->connect(header, &QHeaderView::sectionResized, view, &entity_view::column_resized),
            view->connect(header, &QHeaderView::sectionMoved, view, &entity_view::column_moved),
            view->connect(header, &QHeaderView::sectionCountChanged, view, &entity_view::column_count_changed),
            view->connect(header, &QHeaderView::sectionHandleDoubleClicked, view, &entity_view::column_handle_double_clicked)
            // view->connect(header, &QHeaderView::geometriesChanged, view, &entity_view::updateGeometries)
        };

        // sorting is always enabled for entity_view
        header->setSortIndicatorShown(true);
        header->setSectionsClickable(true);

        if (model)
        {
            view->sort_by_column(header->sortIndicatorSection(), header->sortIndicatorOrder());
        }
        
        sortheader_conn = view->connect(header, &QHeaderView::sortIndicatorChanged, view, &entity_view::sort_by_column, Qt::UniqueConnection);
        view->updateGeometry();
    }

    void entity_view::list_mode::hide_column(int col)
    {
        if (header->isSectionHidden(col))
        { return; }
        header->hideSection(col);
        view->doItemsLayout();
    }

    void entity_view::list_mode::show_column(int col)
    {
        if (!header->isSectionHidden(col))
        { return; }
        header->showSection(col);
        view->doItemsLayout();
    }

    void entity_view::list_mode::resize_column_to_contents(int col)
    {
        // see qtreeview.cpp line 2658
        if (col < 0 || col >= header->count())
        { return; }

        int contents = view->sizeHintForColumn(col);
        int head = header->isHidden() ? 0 : header->sectionSizeHint(col);
        header->resizeSection(col, std::max(contents, head));
    }

    bool entity_view::list_mode::try_create_node(const QModelIndex& index, const int depth)
    {
        using enum list_mode_tree_node::node_state;
        const bool has_children = model->hasChildren(index);
        bool node_auto_expanded = false;
        
        if (!nodes.contains(index) && has_children)
        {
            create_node(index, Collapsed, depth);
        }

        if (model_is_entity_model && has_children && autoexpand_nodes.size())
        {
            if (const QString id = entity_model_ptr()->id_for_index(index); autoexpand_nodes.contains(id))
            {
                qDebug() << "entity_view: try_create_node: autoexpand: " << id;
                autoexpand_nodes.erase(id);
                expand_node(index);
                node_auto_expanded = true;
            }
        }
        return node_auto_expanded;
    }

    void entity_view::list_mode::create_node(const QModelIndex& index, list_mode_tree_node::node_state state, int depth)
    {
        if (nodes.contains(index))
        { return; }

        using enum list_mode_tree_node::node_state;

        const QModelIndex ancestor = index.parent();
        nodes.insert(index, list_mode_tree_node{
            .items = entity_view_item_array(),
            .parent = nodes.contains(ancestor) ? ancestor : root_index,
            .state = Collapsed,
            .depth = depth,
            .pending_expand = false
        });

        if (state == Expanded)
        { expand_node(index); }
    }

    void entity_view::list_mode::expand_node(const QModelIndex& index)
    {
        if (!nodes.contains(index))
        { return; }

        using enum list_mode_tree_node::node_state;
        if (nodes[index].state == Expanded)
        { return; }
        
        const int count = model->rowCount(index);
        const int delta = count - static_cast<int>(nodes[index].items.size());
        nodes[index].items.reserve(count);

        for (int i = static_cast<int>(nodes[index].items.size()); i < count; ++i)
        {
            if (const QModelIndex index_for_item = model->index(i, active_column, index); index_for_item.isValid())
            {
                nodes[index].items.append(i);

                if (model->hasChildren(index_for_item))
                { create_node(index_for_item, Collapsed, nodes[index].depth + 1); }

                ++view_loaded_items;
            }
        }

        expanded_nodes.insert(index, expanded_node{
            .children{},
            .total_count = count
        });
        if (index != root_index)
        {
            expanded_nodes[nodes[index].parent].children.emplace(index.row(), index);
            expanded_nodes[nodes[index].parent].total_count += count;
        }
        
        const int height_increase = static_cast<int>(nodes[index].items.size()) * default_item_rect_height();
        total_content_height += height_increase;
        visible_items += count;

        if (delta)
        { applied_global_positions.reserve(visible_items); }

        nodes[index].state = Expanded;

        emit view->expanded(index);
    }

    void entity_view::list_mode::autoexpand_child_nodes(const QModelIndex& index)
    {
        std::deque<QModelIndex> to_check;
        for (to_check.push_front(index); to_check.size(); to_check.pop_front())
        {
            const QModelIndex index = to_check.front();
            for (int i = 0; std::cmp_less(i, nodes[index].items.size()); ++i)
            {
                // dont take any references here. expand_node() might cause memory reallocation
                if (const QModelIndex maybe_parent = model->index(i, 0, index); try_create_node(maybe_parent, nodes[index].depth + 1))
                { to_check.push_back(maybe_parent); }
            }
        }
    }

    void entity_view::list_mode::clear_node(const QModelIndex& index)
    {
        if (nodes.contains(index))
        {
            view_loaded_items -= static_cast<int>(nodes[index].items.size());
            nodes[index].items.clear();
        }
    }

    void entity_view::list_mode::collapse_node(const QModelIndex& index)
    {
        if (!nodes.contains(index))
        { return; }

        using enum list_mode_tree_node::node_state;

        if (nodes[index].state == Collapsed)
        { return; }

        const int count = static_cast<int>(nodes[index].items.size());
        if (index != root_index)
        {
            expanded_nodes[nodes[index].parent].children.erase(index.row());
            expanded_nodes[nodes[index].parent].total_count -= count;
        }
        expanded_nodes.remove(index);

        const int height_decrease = count * default_item_rect_height();
        total_content_height -= height_decrease;
        visible_items -= count;
        nodes[index].state = Collapsed;

        emit view->collapsed(index);
    }

    // assumes index occurs in tree
    int entity_view::list_mode::content_height_for_node(const QModelIndex& index) const
    {
        const int h = default_item_rect_height();
        int total_h = 0;
        traverse_tree([&](const QModelIndex& parent) -> bool
        {
            total_h += static_cast<int>(nodes[parent].items.size()) * h;
            return false;
        }, index);
        return total_h;
    }

    void entity_view::list_mode::recalculate_content_height()
    {
        total_content_height = content_height_for_node(root_index);
    }

    std::tuple<int, int> entity_view::list_mode::intersecting_range(const QRect& r) const
    {
        const int y = r.top();
        const int h = default_item_rect_height(); // max thumb size is never zero
        const int start = (y / h);
        const int end = std::min(start + (r.height() / h) + 1, visible_items);

        return { start, end };
    }

    // form1 callback int parameters: gstart, lstart, range
    // assumes root occurs in the tree
    template <typename Func, typename T> requires
    detail::maybe_qmodelindex<T> && 
    (
        (sz::is_indicating_function<Func, const QModelIndex&, int, int, int> && std::same_as<T, detail::nulltype<QModelIndex>>) ||
        sz::is_indicating_function<Func, const QModelIndex&>
    )
    void entity_view::list_mode::traverse_tree(Func callback, const T& root) const
    {
        constexpr bool form1 = std::invocable<Func, const QModelIndex&, int, int, int>;
        constexpr bool form2 = std::invocable<Func, const QModelIndex&>;
        std::stack<std::tuple<const QModelIndex, int>> s;
        QSet<QModelIndex> seen;
        QModelIndex start_index = root_index;
        int gpos = 0;
        if constexpr (form2 && std::same_as<T, QModelIndex>)
        { start_index = root; }
        for (s.push({start_index, 0}); s.size(); )
        {
            auto& [index, pos] = s.top();
            int j = 0;
            bool do_next = false;
            auto it = expanded_nodes.find(index);
            if (it != expanded_nodes.end())
            {
                for (const auto& [i, child] : (*it))
                {
                    if (seen.contains(child))
                    {
                        ++j;
                        continue;
                    }
                    if constexpr (form1)
                    {
                        if (callback(index, gpos, pos, i - pos))
                        { return; }
                        gpos += (i - pos) + 1; // take the distance between child and last recorded pos
                        pos = i + 1; // when we return to 'index', pos will point at the item in 'index' just after the previously popped index
                    }
                    seen.insert(child);
                    s.push({child, 0});
                    do_next = true;
                    break;
                }
                if (do_next)
                { continue; }
                if constexpr (form1)
                {
                    if (callback(index, gpos, pos, nodes[index].size() - pos - 1))
                    { return; }

                    if (auto n = nodes.find(index); j != 0 && std::cmp_equal(j, it->children.size()))
                    {
                        gpos += (n->size() - pos) - 1;
                    }
                    else if (it->children.empty())
                    {
                        gpos += n->size() - 1;
                    }
                    ++gpos;
                }
                else if constexpr (form2)
                {
                    if (callback(index))
                    { return; }
                }
            } // if (it != expanded_nodes.end())
            s.pop();
        }
    }
    
    // depth first
    // assumes root occurs in the tree
    template <typename Func> requires sz::is_indicating_function<Func, const QModelIndex&>
    void entity_view::list_mode::traverse_tree(Func callback, const QModelIndex& root, const bool visit_collapsed) const
    {
        using enum list_mode_tree_node::node_state;
        std::stack<std::tuple<const QModelIndex, int>> s;
        for (s.push({root, 0}); s.size(); )
        {
            auto& [parent, pos] = s.top();
            bool do_next = false;
            const int count = (*nodes.find(parent)).size();
            for ( ; std::cmp_less(pos, count); )
            {
                const QModelIndex idx = model->index(pos++, active_column, parent);
                if (auto it = nodes.find(idx); it != nodes.end())
                {
                    if (visit_collapsed || it->state == Expanded)
                    {
                        s.push({idx, 0});
                        do_next = true;
                        break;
                    }
                }
            }
            if (do_next)
            { continue; }
            if (callback(parent))
            { return; }
            s.pop();
        }
    }

    // purpose built for item layouts
    // assumes root occurs in the tree
    template <typename Func> requires sz::is_indicating_function<Func, const QModelIndex&, entity_view_item&, int>
    void entity_view::list_mode::traverse_tree(Func callback, const QModelIndex& root, int offset, int gstart)
    {
        using enum list_mode_tree_node::node_state;
        std::stack<std::tuple<const QModelIndex, int>> s;
        int gpos = gstart;
        for (s.push({root, offset}); s.size(); )
        {
            auto& [parent, pos] = s.top();
            bool do_next = false;
            auto& items = (*nodes.find(parent)).items;
            for ( ; std::cmp_less(pos, items.size()); ) // walk the items in this node, looking for another expanded parent
            {
                const int i = pos++;
                ++gpos;
                if (callback(parent, items[i], i))
                { return; }

                const QModelIndex idx = model->index(i, 0, parent);
                if (auto it = nodes.find(idx); it != nodes.end() && it->state == Expanded)
                {
                    s.push({idx, 0});
                    do_next = true;
                }
                if (do_next)
                { break; }
            }
            if (do_next)
            { continue; }

            // note that we can't pop from s first here because it would destroy the 'parent' index reference
            if (s.size() == 1 && parent != root_index)
            {
                s.pop();
                // relies on first overload of traverse_tree()
                if (const auto [next, loc] = item_at_global_position(gpos); loc > -1)
                { s.push({next, loc}); }
            }
            else
            { s.pop(); }
        }
    }

    // this would cause an endless loop if root was a fully invalid index
    template <typename Func> requires sz::is_indicating_function<Func, const QModelIndex&>
    void entity_view::list_mode::backtrack(const QModelIndex& index, Func callback) const
    {
        for (QModelIndex idx = index; nodes.contains(idx); idx = nodes[idx].parent)
        {
            if (callback(idx))
            { return; }
        }
    }

    inline void entity_view::list_mode::clear_layout_data()
    {
        bsp.clear();
        applied_global_positions.clear();
    }

    inline std::tuple<QModelIndex, int> entity_view::list_mode::item_at_global_position(int position) const
    {
        QModelIndex parent{};
        int loc = -1;
        traverse_tree([&](const QModelIndex& index, int gstart, int lstart, int range) -> bool
        {
            if (gstart <= position && position <= (gstart + range))
            {
                parent = index;
                loc = lstart + (position - gstart);
                return true;
            }
            return false;
        });
        return { parent, loc };
    }

    inline QRect entity_view::list_mode::branch_rect_for_index(const QModelIndex& index)
    {
        const QRect item_rect = rect_for_model_index(index);
        const int x = item_rect.left() - x_indent_scale;
        return QRect(QPoint(x, item_rect.top()), QSize(x_indent_scale, item_rect.height()));
    }

    inline QRect entity_view::list_mode::rect_for_auxiliary_item(const QModelIndex& index) const
    {
        if (index.column() == active_column)
        { return rect_for_model_index(index); }

        const int y = nodes[index.parent()].items[index.row()].y();
        const int col = index.column();
        return QRect(
            QPoint(header->sectionPosition(col), y),
            QSize(header->sectionSize(col), default_item_rect_height())
        );
    }

    inline void entity_view::list_mode::draw_row(QPainter* painter, const QModelIndex& index)
    {
        painter->save();
        const auto font = view->fontMetrics();
        
        QColor color = view->palette().color(QPalette::Inactive, QPalette::Text);
        color.setAlpha(150);
        painter->setPen(color);
        
        QTextOption textopt;
        textopt.setWrapMode(QTextOption::WrapAnywhere);
        textopt.setTextDirection(Qt::LayoutDirection::LeftToRight);
        textopt.setAlignment(QStyle::visualAlignment(Qt::LayoutDirection::LeftToRight, Qt::AlignVCenter));

        for (int col = 0; col < model->columnCount(index.parent()); ++col)
        {
            if (col == active_column) // the "primary item" is drawn by the delegate
            { continue; }
            
            const QModelIndex idx = model->index(index.row(), col, index.parent());
            if (const QString text = model->data(idx, Qt::DisplayRole).toString(); text.size())
            {
                const QRect col_rect = map_to_viewport(rect_for_auxiliary_item(idx));
                painter->drawText(col_rect, font.elidedText(text, Qt::TextElideMode::ElideRight, col_rect.width()), textopt);
            }
        }
        painter->restore();
    }

    inline void entity_view::list_mode::draw_branch(QPainter* painter, const QStyleOptionViewItem& opt)
    {
        // https://forum.qt.io/topic/161664/qproxystyle-pe_indicatorbranch-drawing-logic/2
        const QRect& rect = opt.rect;
        const auto state = opt.state;
        int xmid = rect.center().x();
        int ymid = rect.center().y();
        int dot_radius = 4;
        QRect indicator_rect(xmid - 5, ymid - 5, 10, 10);
        if (state & QStyle::State_Children) 
        {
            painter->setPen(opt.palette.dark().color());
            painter->drawRect(indicator_rect);
            if (state & QStyle::State_Open) 
            {
                painter->setPen(Qt::blue);
                painter->drawEllipse(QPoint(xmid, ymid), dot_radius, dot_radius);
            } else 
            {
                painter->setPen(QColor(0, 0, 0));
                painter->drawLine(0, ymid, rect.right(), ymid);
            }
        }
        if (state & QStyle::State_Item) 
        {
            QBrush brush(opt.palette.dark().color(), Qt::SolidPattern);
            painter->setPen(QPen(brush, 2.0));
            painter->drawEllipse(QPoint(xmid, ymid), dot_radius / 2, dot_radius / 2);
            if (state & QStyle::State_Sibling) 
            {
                painter->drawLine(xmid, rect.top(), xmid, rect.bottom());
            } 
            else
            {
                painter->drawLine(xmid, rect.top(), xmid, ymid);
                painter->drawLine(xmid, ymid, rect.right(), ymid);
            }
        } 
        else 
        {
            painter->setPen(QPen(QColor(0, 150, 150), 4));
            if (state & QStyle::State_Sibling)
            {
                painter->drawLine(xmid, rect.top(), xmid, rect.bottom());
            }
        }
    }

    inline void entity_view::list_mode::create_bsp()
    {
        const int n = view_loaded_items;
        bsp.create(n);
        bsp.init(view->viewport()->rect() | QRect(QPoint(), approximate_content_size(n)), bsp_node_type::X_Plane);
    }

    inline list_mode_tree_node& entity_view::list_mode::node_for_model_index(const QModelIndex& index)
    {
        if (!nodes.contains(index.parent()))
        {
            return nodes[root_index];
        }
        return nodes[index.parent()];
    }

    inline entity_view_item_array& entity_view::list_mode::array_for_model_index(const QModelIndex& index)
    {
        if (!nodes.contains(index.parent()))
        {
            nodes[root_index].items;
        }
        return nodes[index.parent()].items;
    }

    inline entity_view_item& entity_view::list_mode::viewitem_for_model_index(const QModelIndex& index)
    {
        const QModelIndex& parent = (nodes.contains(index.parent()) ? index.parent() : QModelIndex(root_index));
        entity_view_item_array& items = nodes[parent].items;

        if (std::cmp_less(index.row(), items.size()))
        {
            return items[index.row()];
        }

        qWarning() << "entity_view::list_mode: in call to viewitem_for_model_index(): index.row was out of bounds for its associated item array";
        return items[items.size() - 1];
    }

    inline int entity_view::list_mode::y_hint(int global_position) const
    {
        return global_position * default_item_rect_height();
    }

    inline int entity_view::list_mode::default_item_rect_width() const
    {
        return view->viewport()->rect().width() - 4; // TODO
    }

    inline int entity_view::list_mode::default_item_rect_height() const
    {
        return item_max_thumbnail_size.height() + padding_y + row_spacing_y;
    }

    inline QSize entity_view::list_mode::approximate_content_size(size_t item_count) const
    {
        return QSize(view->viewport()->rect().width(), default_item_rect_height() * static_cast<int>(item_count));
    }

} // namespace rin