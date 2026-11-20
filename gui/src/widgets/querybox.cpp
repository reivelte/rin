// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <QtGui/QResizeEvent>
#include "../window.hpp"
#include "querybox.hpp"

namespace rin
{
    querybox_container::querybox_container(querybox* qb) :
        QFrame(qb, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint),
        m_querybox(qb), m_view(nullptr)
    {
        // setFocusPolicy(Qt::FocusPolicy::NoFocus);
        setAttribute(Qt::WA_ShowWithoutActivating);

        setFrameStyle(QFrame::Shape::Box | QFrame::Shadow::Plain);
        setLineWidth(1);

        m_view = new entity_view(this);
        m_view->set_viewmode(entity_view_mode::List);
        m_view->set_header_enabled(false);
        m_view->set_iconsize(QSize(32, 32));
        m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
        
        auto* model = m_querybox->model();
        m_view->setModel(model);

        connect(m_view, &entity_view::expanded, this, [&]() -> void
        {
            QSize s = m_querybox->attached_window()->size();
            int h = m_view->content_size(s).height();

            h = std::min(qCeil(static_cast<qreal>(s.height()) * 0.5), h);
            resize(width(), h);
        });

        connect(m_view, &entity_view::clicked, this, [&](const QModelIndex& idx) -> void
        {
            qDebug() << m_querybox->model()->data(idx, Qt::DisplayRole).toString();
        });
    }

    querybox_container::~querybox_container()
    {
    }

    void querybox_container::set_root_index(const QModelIndex& idx)
    {
        m_view->setRootIndex(idx);
    }

    bool querybox_container::eventFilter(QObject* obj, QEvent* event)
    {
        Q_UNUSED(obj);
        using enum QEvent::Type;
        
        auto type = event->type();
        if (type == MouseButtonRelease ||
            type == MouseButtonPress ||
            type == MouseButtonDblClick)
        {
            m_querybox->setFocus();
        }
        return false;
    }

    void querybox_container::resizeEvent(QResizeEvent* event)
    {
        QFrame::resizeEvent(event);
        m_view->resize(event->size());
    }

    querybox::querybox(entity_model* model, main_window* window, QWidget* parent) :
        QLineEdit(parent), m_window(window), m_container(nullptr), m_model(model)
    {
        m_container = new querybox_container(this);
        m_container->hide();

        connect(this, &QLineEdit::returnPressed, this, [&]() -> void
        {
            m_container->hide();
            emit return_pressed();
        });

        connect(this, &querybox::focus_in, this, [&]() -> void
        {
            show_popup();
        });

        connect(this, &querybox::focus_out, this, [&]() -> void
        {
            m_container->hide();
        });

        connect(m_window, &main_window::moved, this, [&]() -> void
        {
            if (m_container->isVisible())
            {
                QPoint p = rect().bottomLeft();
                m_container->move(mapToGlobal(p));
            }
        });

        connect(this, &QLineEdit::textEdited, this, [&](const QString& text) -> void
        {
            // query model for tags similar to text (if text is not a file uri)
            QModelIndex idx = m_model->query("!similar_to:" + text, false);
            m_container->set_root_index(idx);
        });
    }

    querybox::~querybox()
    {
    }

    void querybox::set_text(const QString& text)
    {
        setText(text);
    }

    void querybox::show_popup()
    {
        QSize s = size();
        QPoint p = rect().bottomLeft();
        m_container->setGeometry(QRect(mapToGlobal(p), s));
        m_container->show();
    }

    void querybox::resizeEvent(QResizeEvent* event)
    {
        QLineEdit::resizeEvent(event);
        m_container->resize(event->size());
    }

    void querybox::focusInEvent(QFocusEvent* event)
    {
        QLineEdit::focusInEvent(event);
        emit focus_in();
    }

    void querybox::focusOutEvent(QFocusEvent* event)
    {
        QLineEdit::focusOutEvent(event);
        emit focus_out();
    }

    void querybox::keyPressEvent(QKeyEvent* event)
    {
        QLineEdit::keyPressEvent(event);
    }

    entity_model* querybox::model() const
    {
        return m_model;
    }

    main_window* querybox::attached_window() const
    {
        return m_window;
    }

} // namespace rin
