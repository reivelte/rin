// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QSize>
#include <QtCore/QThread>
#include <QtCore/QModelIndex>
#include <QtWidgets/QWidget>

namespace rin
{
    // TODO: inherit from QThread
    // TODO: read from memory cache
    // TODO: read from thumbnail cache on disk
    // TODO: save generated thumbnail to on-disk cache
    // TODO: better determine the scaled size for a thumbnail in the event the thumbnail is not already on disk
    class thumbnail_manager : public QObject
    {
        Q_OBJECT
        
        public:

        static bool has_thumbnail(const QString& path);

        thumbnail_manager();
        void do_job(const QString& file_path, const QModelIndex& index, const QSize& size);
        void do_job(const std::vector<std::tuple<QString, QModelIndex, QSize>>& in);

        public slots:
        void generate_thumbnails(const std::vector<std::tuple<QString, QModelIndex, QSize>>& in);

        signals:
        void thumbnail_generated(const std::tuple<QString, QModelIndex>& ent, const QPixmap& pixmap);
        void thumbnails_generated(const std::vector<std::tuple<QString, QModelIndex, QPixmap>>& data);
    };

}
