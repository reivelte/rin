// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <QtCore/QString>
#include <QtWidgets/QTextEdit>

namespace rin
{
    class resizing_textedit : public QTextEdit
    {
        Q_OBJECT

        public:
        resizing_textedit(const QString& text, QWidget* parent, int max_size, bool limit_width);

        void resize_to_contents();

        signals:
        void editing_finished();

        protected:
        void keyPressEvent(QKeyEvent* e) override;
        void paintEvent(QPaintEvent* e) override;

        private:
        int m_paragraph_height() const;

        private:
        int m_max_size;
        bool m_limit_width;

    };
} // namespace rin
