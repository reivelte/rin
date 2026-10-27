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
        m_image(QPixmap()), m_image_rect(),
        m_target_size(std::nullopt),
        m_aspect(0)
    {
        setFrameStyle(QFrame::Shape::Box | QFrame::Shadow::Plain);
        setLineWidth(1);
        
        if (!img.isNull())
        { set_image(img); }
    }

    ui_image::~ui_image()
    {
    }

    void ui_image::set_target_size(std::optional<sz::thumbnail_target_size> target_size)
    {
        if (target_size)
        {
            m_target_size = *target_size;
            m_resize_to_target(*m_target_size);
        }
        else
        { m_target_size = std::nullopt; }
    }

    void ui_image::set_image(const QPixmap& img)
    {
        m_image = img;
        if (m_image.isNull())
        {
            m_resize_to_target(m_target_size ? *m_target_size : Large_256px);
            m_image_rect = QRect();
        }
        else
        {
            m_aspect = static_cast<qreal>(img.width()) / static_cast<qreal>(img.height());
            if (m_target_size)
            { m_resize_to_target(*m_target_size); }
            else
            { resize(img.size()); }
            m_adjust_image_rect(img);
        }
        update();
    }

    QSize ui_image::sizeHint() const
    {
        if (m_target_size)
        {
            const int s = sz::from_thumbnail_target_size(*m_target_size);
            return {s, s};
        }

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
        if (m_target_size)
        { return sz::from_thumbnail_target_size(*m_target_size); }

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

    void ui_image::m_resize_to_target(sz::thumbnail_target_size target)
    {
        const int s = sz::from_thumbnail_target_size(target);
        resize(s, s);
    }

    // assumes img is not null
    void ui_image::m_adjust_image_rect(const QPixmap& img)
    {
        m_image_rect = QRect(QPoint(), QSize(img.width(), img.height()));
        if (m_target_size)
        {
            m_image_rect.setSize(rin::fit_under(size(), m_image_rect.size()));
            m_image_rect.moveCenter(rect().center());
        }
    }

} // namespace rin
