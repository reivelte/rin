// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QtWidgets/QFrame>

namespace rin
{
    class ui_image : public QFrame
    {
        Q_OBJECT

        public:
        ui_image(QWidget* parent, const QPixmap& img);
        ~ui_image();

        void set_image(const QPixmap& img);

        // reimplemented public functions
        public:
        QSize sizeHint() const override;
        bool hasHeightForWidth() const override;
        int heightForWidth(int w) const override;

        // reimplemented protected functions
        protected:
        void paintEvent(QPaintEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;

        private:
        QPixmap m_image;
        qreal m_aspect;
    };
    
} // namespace rin
