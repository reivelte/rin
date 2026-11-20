// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QtWidgets/QFrame>
#include <QtWidgets/QLineEdit>
#include "../model/entitymodel.hpp"
#include "../view/view.hpp"

namespace rin
{
    class main_window;
    class querybox;

    class querybox_container : public QFrame
    {
        Q_OBJECT
        
        public:
        querybox_container(querybox* qb);
        ~querybox_container();

        void set_root_index(const QModelIndex& idx);

        protected:
        bool eventFilter(QObject* obj, QEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;

        private:
        friend class querybox;
        
        querybox* m_querybox;
        entity_view* m_view;
    };

    class querybox : public QLineEdit
    {
        Q_OBJECT
        
        public:
        querybox(entity_model* model, main_window* window, QWidget* parent = nullptr);
        ~querybox();

        void set_text(const QString& text);
        
        signals:
        void return_pressed();
        void focus_in();
        void focus_out();

        public slots:
        void show_popup();

        protected:
        void resizeEvent(QResizeEvent* event) override;
        void focusInEvent(QFocusEvent* event) override;
        void focusOutEvent(QFocusEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;

        protected:
        entity_model* model() const;
        main_window* attached_window() const;

        private:
        friend class querybox_container;
        
        main_window* m_window;
        querybox_container* m_container;
        entity_model* m_model;
    };

    
} // namespace rin
