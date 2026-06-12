// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <algorithm>
#include <QtGui/QPainterStateGuard>
#include "view.hpp"
#include "impl/iconmode.cc"
#include "impl/listmode.cc"

namespace rin
{
    entity_view::entity_view(QWidget* parent)
    : QAbstractItemView(parent)
    {
        // QWidget
        setMouseTracking(true);
        setAcceptDrops(true);

        // QAbstractScrollArea
        setSizeAdjustPolicy(QAbstractScrollArea::SizeAdjustPolicy::AdjustIgnored);
        setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAsNeeded);
        setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAsNeeded);

        // QAbstractItemView
        setEditTriggers(QAbstractItemView::EditTrigger::SelectedClicked | QAbstractItemView::EditTrigger::EditKeyPressed);
        setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectItems);
        setSelectionMode(QAbstractItemView::SelectionMode::ExtendedSelection);
        setDragDropMode(QAbstractItemView::DragDropMode::DragDrop);

        // entity_view
        set_viewmode(entity_view_mode::List);
    }

    entity_view::~entity_view()
    {
    }

    void entity_view::set_viewmode(entity_view_mode mode)
    {
        if (m && viewmode() == mode) 
        { return; }

        QAbstractItemModel* model = nullptr;
        QModelIndex root_index;
        QSize icon_size;
        QSize listm;
        QSize iconm;
        
        if (m && m->model)
        {
            model = m->model;
            root_index = m->root_index;
            listm = m->list_mode_icon_size;
            iconm = m->icon_mode_icon_size;
        }
        
        using enum entity_view_mode;
        switch (mode)
        {
        case List:
        {
            m = std::make_unique<entity_view::list_mode>(this);
            icon_size = listm;
            m->icon_mode_icon_size = iconm;
            break;
        }
    
        case Icon:
        default:
        {
            m = std::make_unique<entity_view::icon_mode>(this);
            icon_size = iconm;
            m->list_mode_icon_size = listm;
            break;
        }
        }
        
        if (model)
        {
            this->setModel(model);

            m->root_index = root_index;
            QAbstractItemView::setRootIndex(root_index);

            if (m->model_is_entity_model)
            { m->entity_model_ptr()->clear_thumbnails(); }

            if (icon_size.isValid())
            { set_iconsize(icon_size); }
        }

        m->bounds = QRect(viewport()->rect().topLeft() + m->offset(), maximumViewportSize());

        setDragEnabled(mode == List || mode == Icon);

        if (m->model)
        { scheduleDelayedItemsLayout(); }
    }

    void entity_view::set_iconsize(const QSize& size)
    {
        if (size == iconSize())
        { return; }
        
        m->set_iconsize(size); // setIconSize() is called here, triggering a relayout

        if (viewmode() == entity_view_mode::List)
        {
            auto* lv = m->as<list_mode*>();
            lv->clear_layout_data();
            lv->recalculate_content_height();
        }
        else
        { m->clear(); }

        if (m->model_is_entity_model)
        { m->entity_model_ptr()->clear_thumbnails(); }
    }

    entity_view_mode entity_view::viewmode() const
    {
        return m->current_viewmode;
    }

    QList<QModelIndex> entity_view::selected_indexes() const
    {
        return selectedIndexes();
    }

    QList<QModelIndex> entity_view::expanded_indexes() const
    {
        QList<QModelIndex> indexes;
        if (viewmode() == entity_view_mode::List)
        {
            auto* lv = m->as<list_mode*>();
            lv->traverse_tree([&](const QModelIndex& index) -> bool
            {
                indexes << index;
                return false;
            });
        }
        else
        {
            indexes << m->root_index;
        }
        return indexes;
    }

    void entity_view::set_item_alignment(entity_view_item_visual_align align)
    {
        m->item_alignment_in_row = align;
        scheduleDelayedItemsLayout();
    }

    void entity_view::setModel(QAbstractItemModel* model)
    {
        if (model == nullptr)
        { throw std::invalid_argument("model was nullptr"); }

        if (m->model && m->model_is_entity_model)
        {
            // TODO: we might want to be more thorough if we end up switching between different model types at runtime with the same view
            for (auto& conn : m->entitymodel_conns)
            { disconnect(conn); }
        }

        if (m->model)
        {
            for (auto& conn : m->model_conns)
            { disconnect(conn); }
        }

        m->model_is_entity_model = model->inherits("rin::entity_model");
        m->model = model;

        if (viewmode() == entity_view_mode::List)
        {
            auto* lv = m->as<list_mode*>();
            m->block_geometry_updates = true;
            lv->header->setModel(m->model);
            m->block_geometry_updates = false;
            sort_by_column(lv->header->sortIndicatorSection(), lv->header->sortIndicatorOrder());
        }

        if (m->model_is_entity_model)
        {
            auto* entitymodel = m->entity_model_ptr();
            m->entitymodel_conns = {
                connect(this, &entity_view::iconSizeChanged, entitymodel, &entity_model::set_thumbnail_target_size),

                // there is a seriously sinister hidden bug/undefined behavior that corrupts the bsp
                // whenever entity_view's column_resized slot is executed as a result of entity_model sorting itself after a finalized query.
                // list_mode's QHeaderView is signaled to do a variety of things when the model emits layoutChanged, one of which is
                // resizing its last stretched column, emitting a column resized signal that then invokes entity_view::column_resized().
                // when manually resizing the columns, there is no issue, it's only in this special case where there is a chance
                // the bsp gets corrupted. when the corruption happens, the delayed layout we would normally schedule there also mysteriously does not happen.
                // to circumvent this entity_model emits its own sorting-specific signals, which we connect to here.
                // it would be nice if we could just disconnect the header from the model's layoutChanged signal, but Qt makes it a private connection, so doing so
                // would require including their private headers, which is not ideal for many reasons.
                connect(entitymodel, &entity_model::sort_starting, this, &entity_view::prepare_for_layout_change),
                connect(entitymodel, &entity_model::sort_finished, this, &entity_view::layout_changed)
            };
        }

        setItemDelegate(new entity_view_item_delegate(this));
        QAbstractItemView::setModel(model);

        m->model_conns = {
            connect(m->model, &QAbstractItemModel::rowsRemoved, this, &entity_view::rows_removed),
            // connect(m->model, &QAbstractItemModel::layoutAboutToBeChanged, this, &entity_view::prepare_for_layout_change),
            // connect(m->model, &QAbstractItemModel::layoutChanged, this, &entity_view::layout_changed)
        };
    }

    // QAbstractItemView calls this function from its version of setModel. ideally it is not called before the user (rin) sets a model
    void entity_view::setSelectionModel(QItemSelectionModel* model)
    {
        // if (auto* selmodel = selectionModel(); selmodel)
        // {
        //     QObject::disconnect(m->selection_model_conn);
        // }

        QAbstractItemView::setSelectionModel(model);
        // if (auto* selmodel = selectionModel(); selmodel)
        // {
        //     m->selection_model_conn = connect(
        //         selmodel, &QItemSelectionModel::currentRowChanged,
        //         m->model, &QAbstractItemModel::submit
        //     );
        // }

        if (viewmode() == entity_view_mode::List)
        {
            auto* lv = m->as<list_mode*>();
            lv->header->setSelectionModel(model);
        }
    }

    QRect entity_view::visualRect(const QModelIndex &index) const
    {
        return m->map_to_viewport(m->rect_for_model_index(index));
    }

    // TODO: make this configurable, make it work better
    void entity_view::scrollTo(const QModelIndex& index, QAbstractItemView::ScrollHint hint)
    {
        // see qlistview.cpp: line 529
        Q_UNUSED(index);
        Q_UNUSED(hint);
        return;
        // if (!m->index_is_in_scope(index)) { return; }

        // if (const QRect rect = visualRect(index); rect.isValid())
        // {
        //     if (hint == QAbstractItemView::ScrollHint::EnsureVisible && viewport()->rect().contains(rect))
        //     {
        //         viewport()->update(rect);
        //         return;
        //     }

        //     using enum entity_view::Flow;
        //     if (m->flow == TopToBottom)
        //     {
        //         verticalScrollBar()->setValue(m->vertical_scroll_to(index, rect, QAbstractItemView::ScrollHint::PositionAtCenter));
        //     }
        //     if (m->flow == LeftToRight)
        //     {
        //         horizontalScrollBar()->setValue(m->horizontal_scroll_to(index, rect, QAbstractItemView::ScrollHint::PositionAtCenter)) ;
        //     }
        // }
    }

    QModelIndex entity_view::indexAt(const QPoint& point) const
    {
        if (auto indexes = m->intersecting_set(QRect(point.x() + m->horizontal_offset(), point.y() + m->vertical_offset(), 1, 1)); indexes.size())
        {
            if (auto index = std::get<0>(*indexes.rbegin()); index.isValid())
            {
                if (m->interactive_region(index).contains(point))
                { return index; }
            }
        }
        return QModelIndex();
    }

    int entity_view::sizeHintForColumn(int column) const
    {
        int width = 0;
        if (viewmode() == entity_view_mode::List)
        {
            auto* lv = m->as<list_mode*>();
            const auto f = fontMetrics();
            const int thumb_width = m->item_max_thumbnail_size.width();
            lv->traverse_tree([&](const QModelIndex& parent, int gstart, int lstart, int range) -> bool
            {
                Q_UNUSED(gstart);
                for (int i = lstart; i <= (lstart + range); ++i)
                {
                    const QModelIndex index = m->model->index(i, column, parent);
                    const QString text = m->model->data(index, Qt::DisplayRole).toString();
                    const int indent = column == 0 ? lv->nodes[parent].depth * lv->x_indent_scale + lv->x_indent_scale : 0;
                    width = std::max(width, indent + thumb_width + 8 + f.horizontalAdvance(text)); // TODO: remove hardcoded values
                }
                return false;
            });
        }
        return width;
    }

    void entity_view::sort_by_column(int column, Qt::SortOrder order)
    {
        if (column < 0) // we don't support disabling of sort
        { return; }

        if (viewmode() == entity_view_mode::List)
        {
            auto* lv = m->as<list_mode*>();
            lv->header->setSortIndicator(column, order);
        }
        emit sort_indicator_changed(column, order);
        m->model->sort(column, order);
    }

    void entity_view::reset()
    {
        qDebug() << "entity_view: reset";
        m->clear();
        QAbstractItemView::reset();
    }

    void entity_view::setRootIndex(const QModelIndex& index)
    {
        qDebug() << "entity_view: setRootIndex: " << index;
        m->clear();
        clearSelection();
        m->active_column = 0;
        m->root_index = index;
        if (viewmode() == entity_view_mode::List)
        {
            auto* lv = m->as<list_mode*>();
            lv->header->setRootIndex(index);
        }
        QAbstractItemView::setRootIndex(index); // will schedule a layout and updateGeometry()
    }

    void entity_view::selectAll()
    {
        if (auto* selmodel = selectionModel(); selmodel && m->model->hasChildren(m->root_index))
        {
            QItemSelection selection;
            const QModelIndex tl = m->model->index(0, 0, m->root_index);
            const QModelIndex br = m->model->index(m->model->rowCount(m->root_index) - 1, 0, m->root_index);
            selection.append(QItemSelectionRange(tl, br));
            selmodel->select(selection, QItemSelectionModel::SelectionFlag::ClearAndSelect);
        }
    }

    void entity_view::updateGeometries()
    {
        if (m->block_geometry_updates)
        { return; }

        if (viewmode() == entity_view_mode::List)
        {
            auto* lv = m->as<list_mode*>();
            if (lv->header)
            {
                m->block_geometry_updates = true;
                lv->header->show(); // the header will be in a hidden state if we switch to list_mode from icon_mode
                int height = 0;
                height = std::max(lv->header->minimumHeight(), lv->header->sizeHint().height());
                height = std::min(height, lv->header->maximumHeight());
                setViewportMargins(0, height, 0, 0);
                const QRect vg = viewport()->geometry();
                const QRect hg(vg.left(), vg.top() - height, vg.width(), height);
                lv->header->setGeometry(hg);
                QMetaObject::invokeMethod(lv->header, "updateGeometries");
            }
        }
        else
        {
            // undo any changes made by list_mode
            setViewportMargins(0, 0, 0, 0);
        }

        if (geometry().isEmpty() || m->model->rowCount(m->root_index) <= 0 || m->model->columnCount(m->root_index) <= 0)
        {
            horizontalScrollBar()->setRange(0, 0);
            verticalScrollBar()->setRange(0, 0);
        }
        else
        {
            m->content_size = QSize(viewport()->rect().size().width(), m->total_content_height);
            m->update_vertical_scrollbar(QSize(m->item_max_width, m->item_max_height));
        }
        m->block_geometry_updates = false;
        updateEditorGeometries();
    }

    void entity_view::doItemsLayout()
    {
        auto orig_state = state();
        setState(QAbstractItemView::ExpandingState);
        if (m->model->rowCount(m->root_index) && (m->model->columnCount(m->root_index)))
        {
            auto desc = m->prepare_item_layout();
            m->do_item_layout(desc);
        }
        QAbstractItemView::doItemsLayout(); // stops layout timer, update()s viewport, child widgets (updateGeometries())
        setState(orig_state);
    }

    void entity_view::scrollContentsBy(int dx, int dy)
    {
        m->scroll_contents_by(dx, dy, false);
    }

    QModelIndex entity_view::moveCursor(QAbstractItemView::CursorAction cursorAction, Qt::KeyboardModifiers modifiers)
    {
        //TODO
        Q_UNUSED(cursorAction);
        Q_UNUSED(modifiers);
        return QModelIndex();
    }

    int entity_view::horizontalOffset() const
    {
        return m->horizontal_offset();
    }

    int entity_view::verticalOffset() const
    {
        return m->vertical_offset();
    }

    bool entity_view::isIndexHidden(const QModelIndex& index) const
    {
        //TODO
        Q_UNUSED(index);
        return false;
    }

    void entity_view::setSelection(const QRect& rect, QItemSelectionModel::SelectionFlags command)
    { // see qabstractitemview.cpp line 673
        QItemSelectionModel* sel_model = selectionModel();
        if (!sel_model)
        { return; }
        QItemSelection sel;
        const QRect translated_rect = rect.translated(m->horizontal_offset(), m->vertical_offset());
        if (rect.width() == 1 && rect.height() == 1) // single select
        {
            if (const auto indexes = m->intersecting_set(translated_rect); indexes.size())
            {
                if (QModelIndex index = std::get<0>(indexes.back()); index.isValid() && m->index_is_enabled(index))
                { 
                    if (m->interactive_region(index).intersects(rect))
                    { sel.select(index, index); }
                }
            }
        }
        else
        {
            if (state() == QAbstractItemView::State::DragSelectingState) // elasticband
            { 
                sel = m->selection_at(translated_rect); 
            }
            else // logical select (key/mouse click selection)
            { // qlistview.cpp line 1357
                QModelIndex tl, br;
                
                const QRect tlr(rect.left() + m->horizontal_offset(), rect.top() + m->vertical_offset(), 1, 1);
                auto indexes = m->intersecting_set(tlr);
                if (indexes.size())
                { tl = std::get<0>(indexes.back()); }

                const QRect brr(rect.right() + m->horizontal_offset(), rect.bottom() + m->vertical_offset(), 1, 1);
                indexes = m->intersecting_set(brr);
                if (indexes.size())
                { br = std::get<0>(indexes.back()); }

                if ((tl.isValid() && br.isValid()) && m->indexes_are_enabled(std::array{tl, br}))
                {
                    QRect first = m->rect_for_model_index(tl);
                    QRect last = m->rect_for_model_index(br);
                    QRect& left = first;
                    QRect& right = last;
                    if (left.center().x() > right.center().x())
                    { qSwap(left, right); }

                    // the selection rect should be bounded by the item rects at tl and br, regardless of where they occur in the viewport (or if they occur)
                    const int t = std::min(left.top(), right.top());
                    const int l = std::min(left.left(), right.left());
                    const int r = std::max(left.right(), right.right());
                    const int b = std::max(left.bottom(), right.bottom());
                    sel.merge(m->selection_at(QRect(l, t, r - l, b - t)), QItemSelectionModel::SelectionFlag::Select);
                }
            } // logical select (key/mouse click selection)
        }
        sel_model->select(sel, command);
    }

    QRegion entity_view::visualRegionForSelection(const QItemSelection& selection) const
    {
        QRegion sel_region;
        const QRect& viewport_rect = viewport()->rect();
        for (const auto& sel_range : selection)
        {
            if (sel_range.isValid())
            {
                // TODO: support for multi-column
                QModelIndex parent = sel_range.topLeft().parent();
                int c = m->active_column;
                for (int i = sel_range.topLeft().row(); i <= sel_range.bottomRight().row(); ++i)
                {
                    const QRect& r = visualRect(m->model->index(i, c, parent));
                    if (viewport_rect.intersects(r))
                    { sel_region += r; }
                }
            }
        }
        return sel_region;
    }

    QModelIndexList entity_view::selectedIndexes() const
    {
        return selectionModel()->selectedIndexes();
    }

    void entity_view::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
    {
        Q_UNUSED(selected);
        Q_UNUSED(deselected);
        viewport()->update();
        qDebug() << "entity_view: selection changed";
        emit selected_count_changed(selectedIndexes().size()); // selected refers to newly selected items, we want all selected items
    }

    void entity_view::initViewItemOption(QStyleOptionViewItem* option) const
    {
        // mostly what QAbstractItemView does
        option->initFrom(this);
        
        option->state &= ~QStyle::State_MouseOver;
        // On mac the focus appearance follows window activation, not widget activation
        if (!hasFocus())
        { option->state &= ~QStyle::State_Active; }
        option->state &= ~QStyle::State_HasFocus;

        // option->backgroundBrush
        // option->checkState
        option->decorationAlignment = m->thumbnail_alignment;
        option->decorationPosition = m->thumbnail_position;
        option->decorationSize = m->item_max_thumbnail_size; // TODO: reset based on sizeHint returned by model(?) or thumbnail size
        option->displayAlignment = m->text_alignment;
        option->features = QStyleOptionViewItem::ViewItemFeature::WrapText;
        option->font = font();
        // option->icon
        // option->index
        option->locale = locale();
        option->locale.setNumberOptions(QLocale::NumberOption::OmitGroupSeparator);
        option->showDecorationSelected = style()->styleHint(QStyle::StyleHint::SH_ItemView_ShowDecorationSelected, nullptr, this);
        // option->text
        option->textElideMode = m->text_elide_mode;
        // option->viewItemPosition
        option->widget = this;
    }

    QSize entity_view::viewportSizeHint() const
    {
        return QAbstractItemView::viewportSizeHint();
    }

    void entity_view::timerEvent(QTimerEvent* e)
    {
        // (2026-04-04) TODO: batching is not currently supported in any view mode
        // if (e->timerId() == m->pending_layout_timer.timerId())
        // {
        //     if (viewmode() != entity_view_mode::Icon)
        //     {
        //         m->pending_layout_timer.stop();
        //         auto s = state();
        //         setState(QAbstractItemView::ExpandingState);
        //         auto d = m->prepare_item_layout();
        //         if (m->do_item_layout(d))
        //         {
        //             // layout was applied
        //             m->pending_layouts.pop_front();
        //             m->pending_layout_timer.stop();
        //             updateGeometries();
        //             viewport()->update();
        //         }
        //         if (m->pending_layouts.size())
        //         {
        //             m->pending_layout_timer.start(0, this); // do the next pending layout
        //         }
        //         setState(s);
        //     }
        // }
        if (e->id() == m->delayed_repaint_timer.id())
        {
            qDebug() << "entity_view: delayed repaint";
            m->interrupt_delayed_repaint();
            viewport()->update();
        }
        QAbstractItemView::timerEvent(e);
    }

    void entity_view::resizeEvent(QResizeEvent* e)
    {
        const QSize delta = e->size() - e->oldSize();
        if (delta.isNull())
        { return; }

        const QSize s = maximumViewportSize();
        m->bounds = QRect(viewport()->rect().topLeft() + m->offset(), s);
        
        if (state() == QAbstractItemView::State::NoState && delta.width())
        {
            using enum entity_view_mode;
            if (viewmode() == Icon)
            {
                // clear the rows here so prepare_item_layout can execute later
                // the trees and row layout will be redone
                static_cast<entity_view::icon_mode*>(m.get())->rows.clear();
            }
            else if (viewmode() == List)
            {
                auto* lv = static_cast<entity_view::list_mode*>(m.get());
                lv->clear_layout_data();
            }

            scheduleDelayedItemsLayout(); 
        }
        else
        { QAbstractItemView::resizeEvent(e); }
    }

    void entity_view::mousePressEvent(QMouseEvent* e)
    {
        QAbstractItemView::mousePressEvent(e);
        // QAbstractItemView also tracks this, but there doesn't seem to be a way to access it
        m->mouse_pressed_position = e->position().toPoint() + m->offset();
    }

    void entity_view::mouseMoveEvent(QMouseEvent* e)
    {
        if (!isVisible())
        { return; }
        QAbstractItemView::mouseMoveEvent(e);

        auto s = state();

        if (s == DraggingState && !m->drag_is_active)
        {
            // drag just started
            m->drag_is_active = true;
        }
        else if (s != DraggingState && m->drag_is_active)
        {
            // dragging just stopped
            m->drag_is_active = false;
            m->mouse_drag_position = QPoint();
            return;
        }

        // the base class also checks if we're mousing over an item in order to activate drag select, but we want it to activate regardless
        if (e->buttons() & Qt::LeftButton && selectionModel() && s != DraggingState)
        {
            setState(QAbstractItemView::State::DragSelectingState);
            const QModelIndex br = indexAt(m->mouse_pressed_position);
            QItemSelectionModel::SelectionFlags command = selectionCommand(br, e);
            
            // dont take the offset for the selection rect.
            // we want it relative to the viewport rect because setSelection expects rect to not have an offset applied
            QRect sr(m->mouse_pressed_position - m->offset(), e->position().toPoint());
            sr = sr.normalized();
            setSelection(sr, command);
        }

        if (s == DragSelectingState && m->show_elasticband)
        {
            QRect sr(m->mouse_pressed_position, e->position().toPoint() + m->offset());
            sr = sr.normalized();
            const int margin = 2 * style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
            const QRect vpr = sr.united(m->elasticband).adjusted(-margin, -margin, margin, margin);
            viewport()->update(m->map_to_viewport(vpr));
            m->elasticband = sr;
        }

        m->set_hover_index(indexAt(e->position().toPoint()));

    }

    void entity_view::mouseReleaseEvent(QMouseEvent* e)
    {
        QAbstractItemView::mouseReleaseEvent(e);
        
        const bool elasticband_was_valid = m->elasticband.isValid();
        if (m->show_elasticband && elasticband_was_valid)
        {
            const int margin = 2 * style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
            const QRect vpr = m->elasticband.adjusted(-margin, -margin, margin, margin);
            viewport()->update(m->map_to_viewport(vpr));
            m->elasticband = QRect();
        }

        const QPoint mouse = e->position().toPoint();

        if (e->button() & Qt::MouseButton::ForwardButton)
        { 
            emit navigate_request(indexAt(mouse), true); 
        }
        else if (e->button() & Qt::MouseButton::BackButton)
        { 
            emit navigate_request(indexAt(mouse), false); 
        }
        else if (e->button() & Qt::MouseButton::LeftButton)
        {
            if (viewmode() == entity_view_mode::List && !elasticband_was_valid)
            {
                // has a tree node been clicked?
                auto* lv = static_cast<entity_view::list_mode*>(m.get());
                const auto indexes = m->intersecting_set(QRect(QPoint(0, mouse.y() + m->vertical_offset()), QSize(m->bounds.width(), 2)));
                if (indexes.size())
                {
                    const QModelIndex index = std::get<0>(indexes[0]); // we only expect one
                    const QRect branch = m->map_to_viewport(lv->branch_rect_for_index(index));
                    if (branch.contains(mouse))
                    {
                        if (m->model->hasChildren(index))
                        {
                            // don't take references from lv->nodes here
                            qDebug() << "entity_view: try toggle node: " << index;
                            if (lv->nodes.contains(index))
                            {
                                if (lv->nodes[index].state == list_mode_tree_node::node_state::Collapsed)
                                {
                                    if (m->model->rowCount(index))
                                    {
                                        lv->expand_node(index);
                                    }
                                    else if (m->model->canFetchMore(index))
                                    {
                                        m->model->fetchMore(index);
                                        lv->nodes[index].pending_expand = true; // the items will be inserted when rowsInserted() is called
                                    }
                                }
                                else if (lv->nodes[index].state == list_mode_tree_node::node_state::Expanded)
                                {
                                    // collapse all nodes, starting at the deepest expanded node and going up to and including 'index'
                                    lv->traverse_tree([lv](const QModelIndex& parent) -> bool
                                    {
                                        lv->collapse_node(parent);
                                        return false;
                                    }, index);
                                }
                                lv->clear_layout_data();
                                scheduleDelayedItemsLayout();
                            } // if (lv->nodes.contains(index))
                            else
                            {
                                // this is known to happen in specific circumstances with QFileSystemModel
                                qWarning() << "entity_view: index expected to be a parent but was not found in the list of parents";
                                return;
                            }
                        } // if (mapped.has_children)
                    } // if (branch.contains(mouse))
                } // if (indexes.size())
            } // if (viewmode() == entity_view::Mode::List && !elasticband_was_valid)
        } // else if (e->button() & Qt::MouseButton::LeftButton)
        else if (e->button() & Qt::MouseButton::RightButton)    
        {
            // we don't care if the index returned here is valid
            emit rightclicked(indexAt(mouse));
        }
    }

    void entity_view::keyPressEvent(QKeyEvent* e)
    {
        // QModelIndex current = currentIndex();
        // qDebug() << "keyPressEvent(): " << e->key();
        // qDebug() << "flags for currently selected item: " << m->model->flags(current) << ", index: " << current;
        QAbstractItemView::keyPressEvent(e);
    }

    void entity_view::paintEvent(QPaintEvent* e)
    {
        Q_UNUSED(e);
        auto* delegate = itemDelegate();
        QStylePainter painter(viewport());
        QStyleOptionViewItem opt;
        initViewItemOption(&opt);

        const QModelIndex current = currentIndex();
        const QItemSelectionModel* selections = selectionModel();
        const QStyle::State view_state = opt.state;
        const bool focus = (hasFocus() || viewport()->hasFocus()) && current.isValid();
        const bool enabled = (view_state & QStyle::State_Enabled) != 0;

        for (const auto& [index, viewitem_index] : m->intersecting_set(m->bounds))
        {
            Q_UNUSED(viewitem_index);
            QPainterStateGuard paintguard(&painter);
            opt.rect = visualRect(index);
            const bool item_enabled = enabled && m->index_is_enabled(index);
            opt.state = view_state;
            opt.state.setFlag(QStyle::State_Selected, selections && selections->isSelected(index));
            opt.state.setFlag(QStyle::State_Enabled, item_enabled);
            opt.state.setFlag(QStyle::State_HasFocus, focus && current == index);
            opt.state.setFlag(QStyle::State_MouseOver, index == m->hovered_index);
            opt.palette.setCurrentColorGroup(item_enabled ? QPalette::Normal : QPalette::Disabled);
            
            m->paint(painter, opt, index);
            delegate->paint(&painter, opt, index);
        }
        if (state() == QAbstractItemView::State::DraggingState)
        {
            if (const QModelIndex target = indexAt(m->mouse_drag_position); target.isValid() && m->model->hasChildren(target))
            {
                QStyleOption iopt;
                iopt.initFrom(this);
                iopt.rect = visualRect(target);
                painter.drawPrimitive(QStyle::PE_IndicatorItemViewItemDrop, iopt);
            }
        }
        if (m->show_elasticband && m->elasticband.isValid())
        {
            QStyleOptionRubberBand optrb;
            optrb.initFrom(this);
            optrb.shape = QRubberBand::Shape::Rectangle;
            optrb.opaque = false;
            optrb.rect = m->map_to_viewport(m->elasticband).intersected(viewport()->rect().adjusted(-16, -16, 16, 16));
            QPainterStateGuard g(&painter);
            painter.drawControl(QStyle::ControlElement::CE_RubberBand, optrb);
        }
    }

    // QAbstractItemView gives us inaccurate positions for both list and icon mode, so we reimplement this function
    void entity_view::dropEvent(QDropEvent* e)
    {
        if (dragDropMode() == InternalMove)
        {
            if (e->source() != this || !(e->possibleActions() & Qt::MoveAction))
            { return; }
        }

        const QPoint mouse = e->position().toPoint();
        QModelIndex index;
        if (viewport()->rect().contains(mouse))
        {
            index = indexAt(mouse);
            if (!index.isValid())
            { index = m->root_index; }
        }

        Qt::DropAction action = e->dropAction();
        bool valid_drop = true;
        if (e->source() == this)
        {
            action = Qt::MoveAction;
            auto selected = selectedIndexes();
            for (QModelIndex child = index; child.isValid() && child != m->root_index; child = child.parent())
            {
                if (selected.contains(child))
                { valid_drop = false; }
            }
        }

        if (valid_drop && !m->model->canDropMimeData(e->mimeData(), action, -1, -1, index))
        {
            // it is assummed that index's parent always accepts drops
            // 2026-03-24: however, the model will still refuse if, for whatever reason, this is not the case (i.e. parent is an indexed query or concept entity)
            index = index.parent();
        }

        if (valid_drop && (m->model->supportedDropActions() & action))
        {
            // we always want to drop on index
            if (m->model->dropMimeData(e->mimeData(), action, -1, -1, index)) // success
            {
                if (action != e->dropAction())
                {
                    e->setDropAction(action);
                    e->accept();
                }
                else
                { e->acceptProposedAction(); }
            }
        }
        
        stopAutoScroll();
        setState(QAbstractItemView::State::NoState);
        viewport()->update();
    }

    void entity_view::dragMoveEvent(QDragMoveEvent* e)
    {
        m->mouse_drag_position = e->position().toPoint();
        QAbstractItemView::dragMoveEvent(e);
    }

    bool entity_view::viewportEvent(QEvent* e)
    {
        return QAbstractItemView::viewportEvent(e);
    }

    void entity_view::wheelEvent(QWheelEvent* e) 
    {
        m->set_hover_index(QModelIndex());
        QAbstractScrollArea::wheelEvent(e);
    }

    void entity_view::dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles)
    {
        // TODO
        QAbstractItemView::dataChanged(topLeft, bottomRight, roles);
        viewport()->update();
    }
    
    void entity_view::rowsInserted(const QModelIndex& parent, int start, int end)
    {
        qDebug() << "entity_view: rowsInserted: " << parent << ", start: " << start << ", end: " << end;
        qDebug() << "entity_view: rowsInserted: root is " << m->root_index;
        if (viewmode() == entity_view_mode::List)
        {
            using enum list_mode_tree_node::node_state;

            auto* lv = m->as<list_mode*>();
            if ((parent.isValid() || parent == m->root_index) && lv->nodes.contains(parent))
            {
                // do not take references from lv->nodes here.
                const bool already_expanded = lv->nodes[parent].state == Expanded;
                if (lv->nodes[parent].pending_expand || already_expanded)
                {
                    if (already_expanded)
                    {
                        // it's fine if parent == root_index because we re-expand after this
                        lv->collapse_node(parent);
                    }
                    lv->expand_node(parent);
                    lv->clear_layout_data();
                    lv->nodes[parent].pending_expand = false;
                }
            }
            else
            {
                if (lv->nodes.empty())
                {
                    qDebug() << "entity_view: nodes was empty, regenerating root";
                    lv->create_node(m->root_index, Expanded, 0);
                    // it is assumed the bsp hasn't been created yet. it will be created in prepare_item_layout()
                    
                    if (m->model_is_entity_model && lv->autoexpand_nodes.size())
                    {
                        lv->autoexpand_child_nodes(m->root_index);
                    }
                }
                else if (lv->nodes.contains(parent.parent()))
                {
                    list_mode_tree_node::node_state node_state = Collapsed;
                    if (m->model_is_entity_model)
                    {
                        auto* entitymodel = lv->entity_model_ptr();
                        node_state = lv->autoexpand_nodes.contains(entitymodel->id_for_index(parent)) ? Expanded : Collapsed;
                    }
                    
                    lv->create_node(parent, node_state, lv->nodes[parent.parent()].depth + 1);
                    
                    if (node_state == Expanded)
                    {
                        lv->autoexpand_child_nodes(parent);
                        lv->clear_layout_data();
                    }
                }
                else
                { qDebug() << "entity_view: rowsInserted: doing nothing"; }
            } // if ((parent.isValid() || parent == m->root_index) && lv->nodes.contains(parent))
        } // if (viewmode() == entity_view_mode::List)
        else if (viewmode() == entity_view_mode::Icon && (parent == m->root_index))
        {
            // there is a chance the column count was not set yet, let's check now
            auto* iv = m->as<icon_mode*>();
            if (std::cmp_not_equal(iv->items_per_row(), iv->items.column_count()))
            {
                // don't bother inserting new data now. a full row layout, tree creation, and item size read needs to happen anyways
                scheduleDelayedItemsLayout();
                return;
            }

            qDebug() << "entity_view: doing dynamic item insert: [start: " << start << ", end: " << end << "] parent: " << parent;
            const int num_cols = static_cast<int>(iv->items.column_count());
            const int num_rows = iv->row_count_for_item_count(m->view_loaded_items + ((end + 1) - start), num_cols);
            const int new_rows_to_add = num_rows - static_cast<int>(iv->rows.size());

            if (!iv->bsp.initialized()) // we only check one because both trees are always cleared and created together
            { iv->init_trees(viewport()->rect(), m->model->rowCount(parent)); }
            
            if (new_rows_to_add)
            {
                // add them in now
                iv->rows.resize(num_rows);
                iv->items.resize(num_rows);
            }

            const auto [min_row_x, spacing_x] = iv->minimum_row_x(viewport()->rect(), num_cols);
            const int row_width = iv->default_row_width(num_cols, spacing_x);
            const int start_row = start / num_cols;
            int h = iv->rows[start_row].height; // 0 if the first item to insert is leading the row
            int y = (start_row == 0 ? m->row_spacing_y : iv->rows[start_row - 1].rect.bottom() + m->row_spacing_y);

            for (int i = start; i < (end + 1); ++i)
            {
                const int row_idx = i / num_cols;
                const int col_idx = i % num_cols;
                const QSize s = m->item_size_from_model(parent, 0, i);
                iv->items[i].resize(s);
                h = s.height() > h ? s.height() : h;
                m->view_loaded_items += 1;
                if (col_idx == num_cols - 1 || (i == end))
                {
                    // we need to layout any new rows and add them to the tree
                    if (iv->bsp_rows.contains(row_idx))
                    { iv->bsp_rows.remove(iv->rows[row_idx].rect, row_idx); }
                    
                    // apply the layout for this row
                    entity_view_item_row& row = iv->rows[row_idx];
                    row.height = h;
                    row.rect = QRect(QPoint(min_row_x, y), QSize(row_width, row.height));
                    row.index = static_cast<size_t>(row_idx) * num_cols;
                    row.count = col_idx + 1;
                    row.hidden = false;
                    iv->bsp_rows.push(row.rect, row_idx);
                    y += (row.height + m->row_spacing_y);
                    h = 0;
                }
            }
            
            m->total_content_height = m->row_spacing_y;
            for (const auto& row : iv->rows)
            { m->total_content_height += (row.height + m->row_spacing_y); }

        } // if (viewmode() == entity_view_mode::Icon)

        scheduleDelayedItemsLayout();
        QAbstractItemView::rowsInserted(parent, start, end);
    }

    void entity_view::rowsAboutToBeRemoved(const QModelIndex& parent, int start, int end)
    {
        using enum list_mode_tree_node::node_state;
        QAbstractItemView::rowsAboutToBeRemoved(parent, start, end);
        
        qDebug() << "entity_view: rowsAboutToBeRemoved: " << parent << ", start: " << start << ", end: " << end;
        
        if (viewmode() == entity_view_mode::List)
        {
            auto* lv = m->as<list_mode*>();
            if (lv->nodes.contains(parent))
            {
                for (int i = start; i <= end; ++i)
                {
                    if (const QModelIndex index = m->model->index(i, 0, parent); lv->nodes.contains(index))
                    {
                        lv->traverse_tree([&](const QModelIndex& idx) -> bool
                        {
                            lv->collapse_node(idx);
                            lv->nodes.remove(idx);
                            return false;
                        }, index, true);
                    }
                }
                auto& items = lv->nodes[parent].items;
                auto b = items.begin() + start;
                auto e = items.begin() + (end + 1);
                items.erase(b, e);
                const int amount_removed = (end - start) + 1;
                m->view_loaded_items -= amount_removed;
                m->total_content_height -= amount_removed * lv->default_item_rect_height();
                lv->clear_layout_data();
            }
        }
        else if (parent == m->root_index)
        { m->clear(); }

        scheduleDelayedItemsLayout();
    }

    void entity_view::prepare_for_layout_change()
    {
        using enum list_mode_tree_node::node_state;
        
        qDebug() << "entity_view: prepare for layout change";
        
        if (viewmode() == entity_view_mode::List && m->model_is_entity_model)
        {
            auto* lv = m->as<list_mode*>();
            auto* entitymodel = m->entity_model_ptr();
            for (const auto [p, n] : lv->nodes.asKeyValueRange())
            {
                if (n.state == Expanded && p != m->root_index)
                {
                    lv->autoexpand_nodes.emplace(entitymodel->id_for_index(p));
                }
            }
        }

        for (const auto& index : selected_indexes())
        {
            m->persistent_selected_indexes.emplace_back(index);
        }
        clearSelection();
        m->clear();
        scheduleDelayedItemsLayout();
    }

    void entity_view::layout_changed()
    {
        qDebug() << "entity_view: layout changed";

        auto* selmodel = selectionModel();
        for (const auto& index : m->persistent_selected_indexes)
        {
            selmodel->select(index, QItemSelectionModel::SelectionFlag::Select);
        }
        m->clear_persistent_selected_indexes();
    }

    void entity_view::rows_removed(const QModelIndex& parent, int start, int end)
    {
        Q_UNUSED(parent);
        Q_UNUSED(start);
        Q_UNUSED(end);
        if (m->model_is_entity_model)
        {
            selectionModel()->clear(); // TODO, this is here to fix a bug where the selection is not updated when dragging items out of the view
        }
    }

    void entity_view::column_resized(int column, int prev_size, int new_size)
    {
        // dont let this function be called as a result of the header emitting signals (as a result of the model emitting signals, i.e. layoutChanged)
        // bad things will happen.
        
        Q_UNUSED(column);
        Q_UNUSED(prev_size);
        Q_UNUSED(new_size);
        // qDebug() << "column_resized: " << column << ", prev: " << prev_size << ", new: " << new_size;
        if (viewmode() == entity_view_mode::List)
        {
            auto* lv = m->as<list_mode*>();
            lv->clear_layout_data();
            scheduleDelayedItemsLayout();
        }
    }

    void entity_view::column_moved()
    {
        // qDebug() << "entity_view: column moved";
        if (viewmode() == entity_view_mode::List)
        {
            auto* lv = m->as<list_mode*>();
            lv->clear_layout_data();
            scheduleDelayedItemsLayout();
        }
    }

    void entity_view::column_count_changed(int prev_count, int new_count)
    {
        // qDebug() << "column count changed: " << prev_count << ", new: " << new_count;
        if (viewmode() == entity_view_mode::List)
        {
            if (prev_count == 0 && new_count > 0)
            {
                scheduleDelayedItemsLayout();
            }

            if (isVisible())
            { updateGeometries(); }

            viewport()->update();
        }
    }

    void entity_view::column_handle_double_clicked(int column)
    {
        if (viewmode() == entity_view_mode::List)
        { m->as<list_mode*>()->resize_column_to_contents(column); }
    }


} // namespace rin

