// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <optional>
#include <QtWidgets/QFrame>
#include <suzuri/utility/thumbnail.hpp>

namespace rin
{
    class ui_image : public QFrame
    {
        Q_OBJECT

        public:
        ui_image(QWidget* parent, const QPixmap& img);
        ~ui_image();

        void set_target_size(std::optional<sz::thumbnail_target_size> target_size);
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
        void m_resize_to_target(sz::thumbnail_target_size target);
        void m_adjust_image_rect(const QPixmap& img);

        private:
        using enum sz::thumbnail_target_size;
        QPixmap m_image;
        QRect m_image_rect; // equal to widget rect if target_size is not set
        std::optional<sz::thumbnail_target_size> m_target_size;
        qreal m_aspect;
    };
    
} // namespace rin
