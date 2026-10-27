// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>
#include "utility/sizing.hpp"
#include "image.hpp"

namespace rin
{
    ui_image::ui_image(QWidget* parent) :
        QFrame(parent),
        m_image(), m_image_rect(),
        m_aspect(0),
        m_auto_resize(true)
    {
        setFrameStyle(QFrame::Shape::Box | QFrame::Shadow::Plain);
        setLineWidth(1);
        
        set_image(QPixmap());
    }

    ui_image::~ui_image()
    {
    }

    void ui_image::set_auto_resize(bool enabled)
    {
        m_auto_resize = enabled;
    }

    void ui_image::set_image(const QPixmap& img)
    {
        m_image = img;
        if (m_image.isNull())
        {
            m_image_rect = QRect();
        }
        else
        {
            m_aspect = static_cast<qreal>(img.width()) / static_cast<qreal>(img.height());

            if (m_auto_resize)
            { resize(img.size()); }

            m_adjust_image_rect(img);
        }
        update();
    }

    QSize ui_image::sizeHint() const
    {
        if (m_image.isNull() || (!m_auto_resize))
        { return QFrame::sizeHint(); }

        return m_image.size();
    }

    bool ui_image::hasHeightForWidth() const
    {
        if (m_auto_resize)
        { return true; }
        
        return false;
    }

    int ui_image::heightForWidth(int w) const
    {
        if (m_auto_resize)
        { return rin::height_for_width(w, m_aspect); }

        return QFrame::heightForWidth(w);
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

        const auto img = icon.pixmap(m_image_rect.size(), icon_mode, icon_state);

        if (selected)
        { painter.fillRect(m_image_rect, palette().brush(cg, QPalette::ColorRole::Highlight)); }
        
        painter.drawPixmap(m_image_rect, img);
    }

    void ui_image::resizeEvent(QResizeEvent* event)
    {
        QFrame::resizeEvent(event);

        if (!m_image.isNull())
        { m_adjust_image_rect(m_image); }
    }

    // assumes img is not null
    void ui_image::m_adjust_image_rect(const QPixmap& img)
    {
        m_image_rect = QRect(QPoint(), QSize(img.width(), img.height()));
        if (!m_auto_resize)
        {
            m_image_rect.setSize(rin::fit_under(size(), m_image_rect.size()));
            m_image_rect.moveCenter(rect().center());
        }
    }

} // namespace rin
