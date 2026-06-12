// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <QtCore/QString>
#include <QtCore/QFileInfo>
#include <QtCore/QList>
#include <QtWidgets/QFrame>
#include <QtWidgets/QFileIconProvider>
#include <QtGui/QTextLayout>

namespace rin
{
    struct navigation_panel_item
    {
        QFileInfo info{};
        QRect rect{};
        bool hovered{};
    };

    class navigation_panel : public QFrame
    {
        Q_OBJECT

        public:
        explicit navigation_panel(QWidget* parent);

        void clear();
        void add_target(const QFileInfo& target);
        void add_targets(const std::vector<std::string>& paths);
        void set_icon_size(QSize size);
        void set_item_spacing(int spacing);

        navigation_panel_item item_at(int index) const;

        QSize sizeHint() const override;

        signals:
        void item_clicked(int index);

        public slots:
        void set_width(int w);

        protected:
        void resizeEvent(QResizeEvent* e) override;
        void paintEvent(QPaintEvent* e) override;
        void mouseReleaseEvent(QMouseEvent* e) override;
        void mouseMoveEvent(QMouseEvent* e) override;
        void leaveEvent(QEvent* e) override;

        private:
        void m_layout_items();
        int m_item_at(QPoint p) const;

        private:
        std::vector<QFileInfo> m_targets;
        std::vector<navigation_panel_item> m_items;
        QFileIconProvider m_icon_provider;
        QSize m_icon_size;
        int m_item_spacing;
        int m_panel_width;
    };

} // namespace rin