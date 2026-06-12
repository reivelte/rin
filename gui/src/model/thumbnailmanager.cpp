// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <QtCore/QFileInfo>
#include <QtGui/QImageReader>
#include <QtGui/QPixmap>
#include <QtConcurrent/QtConcurrentRun>
#include "thumbnailmanager.hpp"

namespace rin
{
    static constexpr std::array<std::string_view, 4> s_valid_thumbnail_types{
        "jpg", "jpeg", "png", "bmp" 
    };

    bool thumbnail_manager::has_thumbnail(const QString& path)
    {
        for (const auto suffix : s_valid_thumbnail_types)
        {
            if (path.endsWith(suffix.data(), Qt::CaseInsensitive))
            { return true; }
        }
        return false;
    }

    thumbnail_manager::thumbnail_manager()
    {
    }

    void thumbnail_manager::do_job(const QString& file_path, const QModelIndex& index, const QSize& size)
    {
        QImageReader reader(file_path);
        reader.setScaledSize(size);
        emit thumbnail_generated({file_path, index}, QPixmap::fromImageReader(&reader)); // calls set_thumbnail inside model
    }

    void thumbnail_manager::do_job(const std::vector<std::tuple<QString, QModelIndex, QSize>>& in)
    {
        std::vector<std::tuple<QString, QModelIndex, QPixmap>> data;
        for (const auto& [path, idx, size] : in)
        {
            if (data.size() >= 50)
            {
                emit thumbnails_generated(data);
                data.clear();
            }
            QImageReader reader(path);
            reader.setScaledSize(size);
            data.emplace_back(path, idx, QPixmap::fromImageReader(&reader));
        }
        if (data.size())
        {
            emit thumbnails_generated(data);
        }
    }

    void thumbnail_manager::generate_thumbnails(const std::vector<std::tuple<QString, QModelIndex, QSize>>& in)
    {   
        QRunnable* worker = QRunnable::create([this, data = in]() -> void { do_job(data); });
        worker->setAutoDelete(true);
        QThreadPool::globalInstance()->start(worker);
    }
}

