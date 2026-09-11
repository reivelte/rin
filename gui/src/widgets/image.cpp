// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>
#include "utility/sizing.hpp"
#include "image.hpp"

namespace rin
{
    ui_image::ui_image(QWidget* parent, const QPixmap& img) :
        QFrame(parent),
        m_image(QPixmap()), m_aspect(0)
    {
        setFrameStyle(QFrame::Shape::Box | QFrame::Shadow::Plain);
        setLineWidth(1);
        
        if (!img.isNull())
        { set_image(img); }
    }

    ui_image::~ui_image()
    {
    }

    void ui_image::set_image(const QPixmap& img)
    {
        m_image = img;
        m_aspect = static_cast<qreal>(img.width()) / static_cast<qreal>(img.height());
        resize(img.width(), img.height());
        update();
    }

    QSize ui_image::sizeHint() const
    {
        if (m_image.isNull())
        { return QFrame::sizeHint(); }

        return m_image.size();
    }

    bool ui_image::hasHeightForWidth() const
    {
        return true;
    }

    int ui_image::heightForWidth(int w) const
    {
        return rin::height_for_width(w, m_aspect);
    }

    void ui_image::paintEvent(QPaintEvent* event)
    {
        if (m_image.isNull())
        {
            QFrame::paintEvent(event);
            return;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);

        // TODO: selected and enabled bool flags
        const bool selected = false;
        const bool enabled = isEnabled();

        QIcon icon(m_image);
        QIcon::Mode icon_mode = QIcon::Mode::Normal;
        QIcon::State icon_state = QIcon::State::On;
        QPalette::ColorGroup cg = QPalette::ColorGroup::Normal;

        if (enabled && selected)
        { icon_mode = QIcon::Selected; }
        else if (!enabled)
        { icon_mode = QIcon::Mode::Disabled; }

        const auto s = m_image.size();
        const auto img = icon.pixmap(s, icon_mode, icon_state);
        const QRect widget_rect = rect();
        const QRect img_rect = QRect(QPoint(), s);

        if (selected)
        { painter.fillRect(img_rect, palette().brush(cg, QPalette::ColorRole::Highlight)); }
        painter.drawPixmap(widget_rect, img, img_rect);
    }

    void ui_image::resizeEvent(QResizeEvent* event)
    {
        QFrame::resizeEvent(event);
    }

} // namespace rin
