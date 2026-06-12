// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <deque>
#include <tuple>
#include <QtCore/QThread>
#include <QtCore/QList>
#include <QtCore/QSet>
#include <QtCore/QBasicTimer>
#include <QtWidgets/QStylePainter>
#include <QtWidgets/QScrollBar>
#include "view/view.hpp"
#include "../delegate.hpp"
#include "../../model/entitymodel.hpp"
#include "viewitem.cc"

namespace rin
{
    struct entity_view_layout_descriptor
    {
        std::vector<QModelIndex> model_indexes{}; // the model items to layout
        QModelIndex parent; // parent of the items to be laid out
        QRect bounding_rect{};
        int position{}; // in the pending_layouts queue
        int y_hint{}; // where to start the layout
        int viewitem_index_start{};
        int viewitem_index_end{}; // [start, end)
    };

    class entity_view::impl
    {
        public:
        entity_view* view;
        QAbstractItemModel* model;
        std::array<QMetaObject::Connection, 1> model_conns;
        std::array<QMetaObject::Connection, 3> entitymodel_conns;
        QMetaObject::Connection selection_model_conn;
        QModelIndex root_index;
        QModelIndex hovered_index; // QAbstractItemView tracks this as well, but there is no way to get the stored hover index from it (that I can see, at least)
        std::deque<entity_view_layout_descriptor> pending_layouts;
        std::vector<entity_view_layout_descriptor> applied_layouts;
        std::vector<QPersistentModelIndex> persistent_selected_indexes; // not cleared in clear()
        QBasicTimer pending_layout_timer;
        QBasicTimer delayed_layout_timer;
        QBasicTimer delayed_repaint_timer;
        QRect elasticband;
        QRect bounds;
        QSize content_size;
        QSize icon_mode_icon_size;
        QSize list_mode_icon_size;
        QSize item_max_thumbnail_size;
        QPoint mouse_pressed_position; // for elasticband (also tracked by QAbstractItemView)
        QPoint mouse_drag_position;
        QStyleOptionViewItem::Position thumbnail_position;
        Qt::Alignment thumbnail_alignment;
        Qt::Alignment text_alignment;
        Qt::TextElideMode text_elide_mode;
        qreal item_aspect_ratio;
        int view_loaded_items; // an item is considered to be loaded if we have a size for it and a row to place it in
        int total_content_height;
        int layout_batch_size;
        int item_default_text_width;
        int item_single_line_text_height;
        int item_text_glyph_width;
        int item_interior_spacing_y;
        int item_max_width;
        int item_max_height;
        int item_min_height; // <= item_max_thumbnail_size.height
        int active_column;
        int8_t row_spacing_y;
        bool show_elasticband;
        bool drag_is_active;
        bool delayed_pending_layout;
        bool model_is_entity_model;
        bool block_geometry_updates;
        entity_view_item_visual_align item_alignment_in_row; // only relevant in icon mode
        entity_view_mode current_viewmode;

        public:
        impl(entity_view* parent);
        virtual ~impl() = default;
        
        /* pure virtual */
        virtual void init() = 0;
        virtual std::vector<std::tuple<QModelIndex, int>> intersecting_set(const QRect& r, bool do_layout = false) = 0;
        virtual entity_view_layout_descriptor prepare_item_layout() = 0;
        virtual bool do_item_layout(const entity_view_layout_descriptor& info = entity_view_layout_descriptor()) = 0;

        /* pure virtual - driving class utility functions */
        virtual QSize item_size_for_model_index(const QModelIndex& index) const = 0;
        virtual QRect rect_for_model_index(const QModelIndex& index) const = 0;

        /* event handlers */
        virtual void paint(QStylePainter& painter, QStyleOptionViewItem& opt, const QModelIndex& index);
        
        /* may or may not be overridden */
        virtual void clear();
        virtual void update_vertical_scrollbar(const QSize& step);
        virtual void scroll_contents_by(int dx, int dy, bool scroll_elastic_band);
        virtual int horizontal_offset() const;
        virtual int vertical_offset() const;
        virtual QRect map_to_viewport(const QRect& r) const;
        virtual int vertical_scroll_to(const QModelIndex& index, const QRect& item_rect, QAbstractItemView::ScrollHint hint);
        virtual int horizontal_scroll_to(const QModelIndex& index, const QRect& item_rect, QAbstractItemView::ScrollHint hint);

        /* templated functions */
        template <typename Ptr>
        inline Ptr as();

        template <typename R> requires (std::ranges::range<R> && std::same_as<std::ranges::range_value_t<R>, QModelIndex>)
        inline bool indexes_are_enabled(const R& indexes);

        /* inline functions */
        inline entity_model* entity_model_ptr();
        inline void clear_persistent_selected_indexes();
        inline QSize item_size_from_model(const QModelIndex& parent, int col, int row) const;
        inline void schedule_delayed_repaint(int delay);
        inline void interrupt_delayed_repaint();
        inline void do_delayed_item_layout(int delay);
        inline void interrupt_delayed_item_layout();
        inline void set_iconsize(QSize s);
        inline QPoint offset() const;
        inline bool index_is_enabled(const QModelIndex& index) const;
        inline QItemSelection selection_at(const QRect& rect);
        inline QRegion interactive_region(const QModelIndex& index) const;
        
        inline bool valid_index(const QModelIndex& index) const;
        inline bool index_is_in_scope(const QModelIndex& index) const;
        inline bool has_rect_for_index(const QModelIndex& index) const;
        inline int flip_x(int x) const;
        inline QPoint flip_x(const QPoint& p) const;
        inline QRect flip_x(const QRect& r) const;
        
        inline void set_hover_index(const QPersistentModelIndex& index);
    };

    entity_view::impl::impl(entity_view* parent) : 
        view(parent), model(nullptr), view_loaded_items(0), total_content_height(0), layout_batch_size(10000), drag_is_active(false), delayed_pending_layout(false),
        model_is_entity_model(false), block_geometry_updates(false)
    {
    }

    void entity_view::impl::paint(QStylePainter& painter, QStyleOptionViewItem& opt, const QModelIndex& index)
    {
        Q_UNUSED(painter);
        Q_UNUSED(opt);
        Q_UNUSED(index);
    }

    void entity_view::impl::clear()
    {
        set_hover_index(QModelIndex());
        interrupt_delayed_item_layout();
        content_size = QSize();
        view_loaded_items = 0;
        total_content_height = row_spacing_y;
        applied_layouts.clear();
        pending_layouts.clear();
        pending_layout_timer.stop();
    }

    void entity_view::impl::update_vertical_scrollbar(const QSize& step) 
    {
        const bool both_scrollbars_auto = (view->verticalScrollBarPolicy() == Qt::ScrollBarAsNeeded) && (view->horizontalScrollBarPolicy() == Qt::ScrollBarAsNeeded);
        QSize size_of_viewport = view->viewport()->size();
        QSize size_of_contents = content_size;

        bool vbar_will_show = size_of_contents.height() > size_of_viewport.height();
        bool hbar_will_show = false;

        view->verticalScrollBar()->setSingleStep(step.height() / 4);
        view->verticalScrollBar()->setPageStep(size_of_viewport.height());

        if (hbar_will_show)
        { vbar_will_show = size_of_contents.height() > size_of_viewport.height() - view->horizontalScrollBar()->height(); }
        
        if (both_scrollbars_auto && !vbar_will_show)
        { view->verticalScrollBar()->setRange(0, 0); } // workaround QTBUG-39902 (see qlistview.cpp line 2037)
        else
        { view->verticalScrollBar()->setRange(0, (size_of_contents.height() - size_of_viewport.height())); }
    }

    void entity_view::impl::scroll_contents_by(int dx, int dy, bool scroll_elastic_band)
    {
        Q_UNUSED(scroll_elastic_band);
        view->scrollDirtyRegion(dx, dy);
        view->viewport()->scroll(dx, dy);
        bounds = QRect(view->viewport()->rect().topLeft() + offset(), bounds.size());
    }

    int entity_view::impl::horizontal_offset() const
    {
        const auto* hbar = view->horizontalScrollBar();
        return (view->isRightToLeft() ? hbar->maximum() - hbar->value() : hbar->value()); // t
    }

    int entity_view::impl::vertical_offset() const
    {
        return view->verticalScrollBar()->value();
    }

    QRect entity_view::impl::map_to_viewport(const QRect& r) const
    {
        // see also qlistview.cpp: line 2722
        QRect mapped;
        if (r.isValid())
        {
            int dx = -horizontal_offset();
            int dy = -vertical_offset();
            mapped = r.adjusted(dx, dy, dx, dy);
        }
        return mapped;
    }

    // TODO
    int entity_view::impl::vertical_scroll_to(const QModelIndex& index, const QRect& item_rect, QAbstractItemView::ScrollHint hint) 
    {
        Q_UNUSED(index);
        Q_UNUSED(hint);
        const QRect area = view->viewport()->rect();
        int v = view->verticalScrollBar()->value();
        
        // position at center
        v += item_rect.top() - ((area.height() - item_rect.height()) / 2);
        return v;
    }

    // TODO
    int entity_view::impl::horizontal_scroll_to(const QModelIndex& index, const QRect& item_rect, QAbstractItemView::ScrollHint hint)
    {
        Q_UNUSED(index);
        Q_UNUSED(item_rect);
        Q_UNUSED(hint);

        return view->horizontalScrollBar()->value();
    }

    template <typename Ptr>
    inline Ptr entity_view::impl::as()
    {
        return static_cast<Ptr>(this);
    }

    /* inline functions */
    template<typename R> requires (std::ranges::range<R> && std::same_as<std::ranges::range_value_t<R>, QModelIndex>)
    inline bool entity_view::impl::indexes_are_enabled(const R& indexes)
    {
        for (const QModelIndex& index : indexes)
        {
            if (!index_is_enabled(index))
            { return false; }
        }
        return true;
    }

    inline entity_model* entity_view::impl::entity_model_ptr()
    {
        if (!model_is_entity_model)
        { qWarning() << "entity_view: tried to interpret assigned model as an entity_model when it is not"; }

        return qobject_cast<entity_model*>(model);
    }

    inline void entity_view::impl::clear_persistent_selected_indexes()
    {
        persistent_selected_indexes.clear();
    }

    inline QSize entity_view::impl::item_size_from_model(const QModelIndex& parent, int col, int row) const
    {
        QStyleOptionViewItem opt;
        view->initViewItemOption(&opt);
        opt.rect = QRect(QPoint(), QSize(item_max_width, item_max_height));
        const QModelIndex index = model->index(row, col, parent);
        const QSize item_size = view->itemDelegateForIndex(index)->sizeHint(opt, index);
        return QSize(std::min(item_size.width(), item_max_width), item_size.height());
    }

    inline void entity_view::impl::schedule_delayed_repaint(int delay)
    {
        delayed_repaint_timer.start(delay, view);
    }

    inline void entity_view::impl::interrupt_delayed_repaint()
    {
        delayed_repaint_timer.stop();
    }

    inline void entity_view::impl::do_delayed_item_layout(int delay)
    {
        if (delayed_pending_layout)
        { return; }

        delayed_pending_layout = true;
        delayed_layout_timer.start(delay, view);
    }

    inline void entity_view::impl::interrupt_delayed_item_layout()
    {
        delayed_pending_layout = false;
        delayed_layout_timer.stop();
    }

    inline void entity_view::impl::set_iconsize(QSize s)
    {
        if (s == item_max_thumbnail_size)
        { return; }
        item_max_thumbnail_size = s;
        item_default_text_width = std::max(item_max_thumbnail_size.width(), 100);
        item_max_width = std::max(item_max_thumbnail_size.width(), item_default_text_width);
        item_aspect_ratio = 2.0 / 3.0; // TODO: make aspect ratio for items configurable
        item_min_height = item_max_thumbnail_size.height();

        QFontMetrics metrics(view->font());
        item_text_glyph_width = metrics.boundingRect("A").width();
        item_single_line_text_height = metrics.height();
        item_max_height = item_min_height + (4 * item_single_line_text_height);

        switch (current_viewmode)
        {
        case entity_view_mode::Icon:
            icon_mode_icon_size = s;
            if (!list_mode_icon_size.isValid())
            { list_mode_icon_size = QSize(32, 32); } // TODO
            break;
        
        case entity_view_mode::List:
        default:
            if (!icon_mode_icon_size.isValid())
            { icon_mode_icon_size = QSize(256, 256); }
            list_mode_icon_size = s;
            break;
        }
        view->setIconSize(s);
    }

    inline QPoint entity_view::impl::offset() const
    { return QPoint(view->isRightToLeft() ? -horizontal_offset() : horizontal_offset(), vertical_offset()); }

    inline bool entity_view::impl::index_is_enabled(const QModelIndex& index) const
    { return (model->flags(index) & Qt::ItemIsEnabled); }

    inline QItemSelection entity_view::impl::selection_at(const QRect& rect)
    { // see qlistview.cpp line 1915
        QItemSelection selection;
        QModelIndex tl, br;
        auto indexes = intersecting_set(rect.normalized(), false);
        for (const auto& [index, viewitem_index] : indexes)
        {
            Q_UNUSED(viewitem_index);

            if (!tl.isValid() && !br.isValid())
            { tl = br = index; }
            else if (index.row() == (tl.row() - 1))
            { tl = index; }
            else if (index.row() == (br.row() + 1))
            { br = index; }
            else
            {
                selection.select(tl, br);
                tl = br = index;
            }
        }
        
        if (tl.isValid() && br.isValid())
        { selection.select(tl, br); }
        else if (tl.isValid())
        { selection.select(tl, tl); }
        else if (br.isValid())
        { selection.select(br, br); }
        
        return selection;
    }

    inline QRegion entity_view::impl::interactive_region(const QModelIndex& index) const
    {
        // same as view->visualRect()
        return map_to_viewport(rect_for_model_index(index));
    }

    inline bool entity_view::impl::valid_index(const QModelIndex& index) const
    { return (index.row() >= 0) && (index.column() >= 0) && (index.model() == model); }

    inline bool entity_view::impl::index_is_in_scope(const QModelIndex& index) const
    { return valid_index(index) && (index.column() == active_column) && (index.parent() == root_index); }

    // TODO
    inline bool entity_view::impl::has_rect_for_index(const QModelIndex& index) const
    {
        return (index_is_in_scope(index));
    }

    inline int entity_view::impl::flip_x(int x) const
    { return std::max(view->viewport()->width(), content_size.width()) - x; }

    inline QPoint entity_view::impl::flip_x(const QPoint& p) const
    { return QPoint(flip_x(p.x()), p.y()); }

    inline QRect entity_view::impl::flip_x(const QRect& r) const
    { return QRect(flip_x(r.left()) - r.width(), r.top(), r.width(), r.height()); }

    inline void entity_view::impl::set_hover_index(const QPersistentModelIndex& index)
    {
        if (hovered_index == index)
        { 
            return; 
        }

        view->update(index);
        view->update(hovered_index);

        hovered_index = index;
    }

} // namespace rin
