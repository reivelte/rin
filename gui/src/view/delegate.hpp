// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <QtCore/QString>
#include <QtWidgets/QStyledItemDelegate>
#include <QtGui/QTextLayout>

namespace rin
{
    class entity_view;
    class entity_view_item_delegate : public QStyledItemDelegate
    {
        Q_OBJECT

        public:
        entity_view_item_delegate(QObject* parent);

        QRegion interactive_region(const QStyleOptionViewItem& option, const QModelIndex& index) const;
        QSize thumbnail_size(const QSize& max_thumbnail_size, const QModelIndex& index) const;

        // reimplemented functions
        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
        QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
        QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
        void setEditorData(QWidget* editor, const QModelIndex& index) const override;
        void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
        void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

        protected:
        bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override;
        
        private:
        int m_approximate_text_height(const QString& text, const QStyleOptionViewItem& opt) const;
        QSize m_layout_text(QTextLayout& layout, const QStyleOptionViewItem& opt, const int max_line_height = -1) const; // returns layout size
        QRect m_thumbnail_rect(const QRect& item, const QSize& size) const;
        QRect m_name_rect(const QRect& item, const QRect& thumb, const QSize& size) const;

        // QStyleOptionViewItem stores this as 'widget'
        // the constructor checks if we are assigned to an entity_view and throws an exception if this is not the case
        // this delegate is designed to only work with entity_view, we store a pointer to it here for convenience
        const entity_view* const m_view;
    };

}
