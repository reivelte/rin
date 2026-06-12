// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <memory>
#include <QtCore/QAbstractItemModel>
#include <QtWidgets/QAbstractItemView>

namespace rin
{
    enum class entity_view_mode : int8_t { Icon, List };
    enum class entity_view_item_visual_align : int8_t { Center, Top, Bottom };

    class entity_view : public QAbstractItemView
    {
        Q_OBJECT

        public:
        explicit entity_view(QWidget* parent);
        ~entity_view();

        void set_viewmode(entity_view_mode mode);
        void set_iconsize(const QSize& size);
        
        entity_view_mode viewmode() const;
        QList<QModelIndex> selected_indexes() const;
        QList<QModelIndex> expanded_indexes() const;

        void set_item_alignment(entity_view_item_visual_align align);

        //reimplemented public functions
        void setModel(QAbstractItemModel* model) override;

        // reimplemented required public functions
        void setSelectionModel(QItemSelectionModel* model) override;
        QRect visualRect(const QModelIndex& index) const override;
        void scrollTo(const QModelIndex& index, QAbstractItemView::ScrollHint hint = QAbstractItemView::ScrollHint::EnsureVisible) override;
        QModelIndex indexAt(const QPoint& point) const override;
        int sizeHintForColumn(int column) const override;

        signals:
        void selected_count_changed(qsizetype count);
        void navigate_request(const QModelIndex& index, bool forward);
        void rightclicked(const QModelIndex& index);
        void sort_indicator_changed(int column, Qt::SortOrder order);
        void expanded(const QModelIndex& index);
        void collapsed(const QModelIndex& index);

        public slots:
        void sort_by_column(int column, Qt::SortOrder order);
        
        // reimplemented slots
        void doItemsLayout() override;
        void reset() override;
        void setRootIndex(const QModelIndex& index) override;
        void selectAll() override;
        void updateGeometries() override;

        protected:
        // reimplemented required functions
        void scrollContentsBy(int dx, int dy) override;
        QModelIndex moveCursor(QAbstractItemView::CursorAction cursorAction, Qt::KeyboardModifiers modifiers) override;
        int horizontalOffset() const override;
        int verticalOffset() const override;
        bool isIndexHidden(const QModelIndex& index) const override;
        void setSelection(const QRect& rect, QItemSelectionModel::SelectionFlags command) override;
        QRegion visualRegionForSelection(const QItemSelection& selection) const override;
        QModelIndexList selectedIndexes() const override;
        void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) override;
        void initViewItemOption(QStyleOptionViewItem* option) const override;
        QSize viewportSizeHint() const override;

        // reimplemented event functions
        void timerEvent(QTimerEvent* e) override;
        void resizeEvent(QResizeEvent* e) override;
        void mousePressEvent(QMouseEvent* e) override;
        void mouseMoveEvent(QMouseEvent* e) override;
        void mouseReleaseEvent(QMouseEvent* e) override;
        void keyPressEvent(QKeyEvent* e) override;
        void paintEvent(QPaintEvent* e) override;
        void dropEvent(QDropEvent* e) override;
        void dragMoveEvent(QDragMoveEvent* e) override;
        bool viewportEvent(QEvent* e) override;
        void wheelEvent(QWheelEvent* e) override;

        protected slots:
        // reimplemented slots
        void dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles = QList<int>()) override;
        void rowsInserted(const QModelIndex& parent, int start, int end) override;
        void rowsAboutToBeRemoved(const QModelIndex& parent, int start, int end) override;
        
        void prepare_for_layout_change();
        void layout_changed();
        void rows_removed(const QModelIndex& parent, int start, int end);
        void column_resized(int column, int prev_size, int new_size);
        void column_moved();
        void column_count_changed(int prev_count, int new_count);
        void column_handle_double_clicked(int column);
        
        private:
        class impl;
        class icon_mode;
        class list_mode;
        friend class impl;
        Q_DISABLE_COPY(entity_view)
        std::unique_ptr<impl> m;
    };
}