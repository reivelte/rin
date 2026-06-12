// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <QtWidgets/QPlainTextEdit>
#include <QtGui/QPainter>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainterStateGuard>
#include "../view/view.hpp"
#include "../utility/sizing.hpp"
#include "../widgets/resizing_textedit.hpp"
#include "delegate.hpp"

namespace rin
{
    entity_view_item_delegate::entity_view_item_delegate(QObject* parent)
    : QStyledItemDelegate(parent), m_view(qobject_cast<const entity_view*>(parent))
    {
        if (m_view == nullptr)
        {
            throw std::runtime_error("view was nullptr");
        }
    }

    QRegion entity_view_item_delegate::interactive_region(const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
        using enum entity_view_mode;
        auto mode = m_view->viewmode();
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        QRegion region;
        QTextLayout name_layout(opt.text);
        const QSize name_layout_size = m_layout_text(name_layout, opt);
        const QRect thumb_rect = m_thumbnail_rect(opt.rect, thumbnail_size(m_view->iconSize(), index));
        const QRect name_rect = m_name_rect(opt.rect, thumb_rect, name_layout_size);
        if (mode == Icon)
        {
            region |= thumb_rect;
            region |= name_rect;
        }
        else
        { region = thumb_rect | name_rect; }
        return region;
    }

    // the view calls this function when it needs to request a thumbnail from the model
    // this function was made before initStyleOption was reimplemented, but it serves as a shortcut to
    // the desired thumbnail size when we don't care about anything else
    QSize entity_view_item_delegate::thumbnail_size(const QSize& max_thumbnail_size, const QModelIndex& index) const
    {
        return rin::fit_under(max_thumbnail_size, index.data(Qt::SizeHintRole).toSize());
    }

    void entity_view_item_delegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
        // REFERENCE: https://code.qt.io/cgit/qt/qtbase.git/tree/src/widgets/styles/qcommonstyle.cpp
        using enum entity_view_mode;
        const entity_view_mode viewmode = m_view->viewmode();
        
        QPainterStateGuard g(painter);
        
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        
        const QRect thumb_rect = m_thumbnail_rect(opt.rect, thumbnail_size(m_view->iconSize(), index));
        const bool is_enabled = opt.state & QStyle::State_Enabled;
        const bool is_selected = opt.state & QStyle::State_Selected;

        const QPalette::ColorGroup cg = is_enabled ? (opt.state & QStyle::State_Active ? QPalette::Normal : QPalette::Inactive) : QPalette::Disabled;
        const QBrush highlight = opt.palette.brush(cg, QPalette::Highlight);
        
        QRect name_rect;
        if (!opt.text.isEmpty())
        {
            // draw the text (see qcommonstyle.cpp: line 2307)
            QTextLayout name_layout;
            if (viewmode == List)
            {
                const int max_len = opt.rect.width() - (thumb_rect.width() + 4); // TODO: remove hardcoded values
                name_layout.setText(opt.fontMetrics.elidedText(opt.text, Qt::TextElideMode::ElideRight, max_len));
            }
            else
            { name_layout.setText(opt.text); }

            const QSize name_layout_size = m_layout_text(name_layout, opt);
            name_rect = m_name_rect(opt.rect, thumb_rect, name_layout_size);

            painter->setPen(opt.palette.color(cg, is_selected ? QPalette::HighlightedText : QPalette::Text));

            if (is_selected)
            { painter->fillRect(name_rect, highlight); }

            // for icon mode, opt.rect.x is used here instead of name_rect.x because the text was centered based on opt.rect.width
            // the text is not centered in list mode, so we can use name_rect.topLeft then
            name_layout.draw(painter, viewmode == Icon ? QPointF(opt.rect.x(), name_rect.top()) : name_rect.topLeft());
        }

        // thumbnail
        if (opt.showDecorationSelected && is_selected) 
        { painter->fillRect(thumb_rect, highlight); }
        
        QIcon::Mode mode = QIcon::Normal;
        if (!(is_enabled))       { mode = QIcon::Disabled; }
        else if (is_selected)    { mode = QIcon::Selected; }
        
        painter->drawPixmap(thumb_rect, opt.icon.pixmap(thumb_rect.size(), mode, opt.state & QStyle::State_Open ? QIcon::On : QIcon::Off));

        // highlight
        if (opt.state & QStyle::State_MouseOver)
        {
            const QBrush hover_highlight(QColor(255, 255, 255, 30));
            if (viewmode == Icon)
            {
                for (const auto& r : std::array<QRect, 2>{thumb_rect, name_rect})
                { painter->fillRect(r, hover_highlight); }    
            }
            else
            { painter->fillRect(thumb_rect | name_rect, hover_highlight); }
        }
    }

    // 2025/02/04: the view no longer relies on this when doing a layout, but it may still be useful
    // the width stays fixed to the value set in the view, if an entity_view requests a sizeHint, the width returned here won't matter
    // it is basically a glorified "heightForWidth()" function
    // entity_view provides a rect as an initial value (based on the max thumbnail size set in the view)
    // there is a chance that all data required to display the item is not available yet (namely thumbnails)
    QSize entity_view_item_delegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
        using enum entity_view_mode;

        if (m_view->viewmode() == List)
        { return option.rect.size(); }

        // opt.decorationSize here may not be accurate if the thumbnail hasn't been loaded yet. lets compute it now
        // decorationSize is always set to iconSize() of view by default
        // entity_model provides the actual size of an image if the index represents an image file on disk
        // if the size returned by index.data() is empty, fit_under() returns the given target_size (m_view->iconSize())
        const QSize thumbsize = rin::fit_under(m_view->iconSize(), index.data(Qt::SizeHintRole).toSize());
        const QString name = index.data(Qt::DisplayRole).toString();
        return QSize(
            option.rect.width(),
            thumbsize.height() + m_approximate_text_height(name, option)
        );
    }

    QWidget* entity_view_item_delegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
        resizing_textedit* editor = nullptr;
        const QString text = index.data(Qt::EditRole).toString();
        const int w = option.rect.width();

        if (m_view->viewmode() == entity_view_mode::Icon)
        {
            editor = new resizing_textedit(text, parent, w, true);
        }
        else
        {
            editor = new resizing_textedit(text, parent, w, false);
        }

        // TODO: find a better way to do this
        auto* this_ = const_cast<entity_view_item_delegate*>(this);
        connect(editor, &resizing_textedit::editing_finished, this_, 
        [this_, editor]() -> void
        {
            emit this_->commitData(editor);
            emit this_->closeEditor(editor);
        });

        updateEditorGeometry(editor, option, index);

        return editor;
    }

    void entity_view_item_delegate::setEditorData(QWidget* editor, const QModelIndex& index) const
    {
        auto* textedit = qobject_cast<resizing_textedit*>(editor);
        textedit->setPlainText(index.data(Qt::EditRole).toString());
        
        // QAbstractItemView won't do it for us if the editor is not one of QLineEdit or QSpinBox/QDoubleSpinBox
        textedit->selectAll();
    }

    void entity_view_item_delegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
    {
        auto* textedit = qobject_cast<resizing_textedit*>(editor);
        const QString text = textedit->toPlainText();
        
        if (text.isEmpty())
        { return; }

        model->setData(index, text, Qt::EditRole);
    }

    void entity_view_item_delegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
        QRect r;
        const QRect thumb = m_thumbnail_rect(option.rect, thumbnail_size(m_view->iconSize(), index));
        
        // fontMetrics.height used as "padding" to prevent part of the text from being cut off when the editor is spawned
        const int th = m_approximate_text_height(index.data(Qt::DisplayRole).toString(), option) + option.fontMetrics.height();
        
        if (m_view->viewmode() == entity_view_mode::List)
        {
            // TODO: remove hardcoded values
            r = m_name_rect(option.rect, thumb, QSize(option.rect.width() - (thumb.width() + 4), th));
        }
        else
        {
            r = m_name_rect(option.rect, thumb, QSize(option.rect.width(), th));
        }
        editor->setGeometry(r);
    }

    bool entity_view_item_delegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index)
    {
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }

    int entity_view_item_delegate::m_approximate_text_height(const QString& text, const QStyleOptionViewItem& opt) const
    {
        const QFontMetrics& font = opt.fontMetrics;
        return rin::approximate_height(
            text,
            opt.rect.width(),
            font.boundingRect("A").toRectF().width(),
            font.height(),
            1
        );
    }

    QSize entity_view_item_delegate::m_layout_text(QTextLayout &layout, const QStyleOptionViewItem &opt, const int max_line_height) const
    {
        // TODO: calculateElidedText()
        QTextOption text_option;
        text_option.setWrapMode(QTextOption::WrapAnywhere);
        text_option.setTextDirection(Qt::LayoutDirection::LeftToRight);
        text_option.setAlignment(opt.displayAlignment);
        // text_option.setAlignment(QStyle::visualAlignment(Qt::LayoutDirection::LeftToRight, Qt::AlignHCenter));

        layout.setTextOption(text_option);

        // qcommonstyle.cpp: line 823: viewItemTextLayout(QTextLayout &textLayout, int lineWidth, int maxHeight = -1, int *lastVisibleLine = nullptr)
        int last_visible_line = -1;
        qreal height = 0;
        qreal width_used = 0;
        int i = 0;
        layout.beginLayout();
        while (true) 
        {
            QTextLine line = layout.createLine();
            
            if (!line.isValid()) 
            { break; }

            line.setLineWidth(opt.rect.width());
            line.setPosition(QPointF(0, height));
            height += line.height();
            width_used = qMax(width_used, line.naturalTextWidth()); // always <= line_width
            
            // we assume that the height of the next line is the same as the current one
            if (max_line_height > 0 && last_visible_line && height + line.height() > max_line_height) 
            {
                const QTextLine next_line = layout.createLine();
                last_visible_line = next_line.isValid() ? i : -1;
                break;
            }
            ++i;
        }
        layout.endLayout();
        return QSize(qCeil(width_used), qCeil(height));
    }

    QRect entity_view_item_delegate::m_thumbnail_rect(const QRect& item, const QSize& size) const
    {
        QRect thumb;
        switch(m_view->viewmode()) {
        case entity_view_mode::Icon:
        {
            thumb = QRect(QPoint(item.x() + ((item.width() - size.width()) / 2), item.y()), size);
            break;
        }
        case entity_view_mode::List:
        {
            thumb = QRect(QPoint(item.x(), item.top() + ((item.height() - size.height()) / 2)), size);
            thumb.moveCenter(QPoint(item.x() + m_view->iconSize().width() / 2, thumb.center().y()));
            break;
        }
        default: break;
        }
        return thumb;
    }

    QRect entity_view_item_delegate::m_name_rect(const QRect& item, const QRect& thumb, const QSize& size) const
    {
        QRect name;
        switch (m_view->viewmode()) {
        case entity_view_mode::Icon:
        {
            const int translate_x = (item.width() - size.width()) / 2;
            name = QRect(QPoint(item.x() + translate_x, thumb.bottom() + 1), size);
            break;
        }
        case entity_view_mode::List:
        {
            name = QRect(QPoint(thumb.right() + 4, item.top() + (((item.height() - size.height()) / 2))), size);
            break;
        }
        default: break;
        }
        return name;
    }
}
