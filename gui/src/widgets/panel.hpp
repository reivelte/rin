// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <unordered_map>
#include <QtWidgets/QFrame>

namespace rin
{
    class ui_panel : public QFrame
    {
        Q_OBJECT
        public:
        enum class grip_edge_position : int8_t { Left, Right };

        public:
        ui_panel(QWidget* parent);
        ~ui_panel();

        void set_panel_width(int w);
        void set_grip_edge_size(grip_edge_position position, int w);

        QSize grip_size(grip_edge_position position = grip_edge_position::Right) const;

        // reimplemented
        QSize sizeHint() const override;

        protected:
        void resizeEvent(QResizeEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;

        private:
        std::unordered_map<grip_edge_position, QSize> m_grip_edges;
        int m_panel_width;
    };
    
} // namespace rin
