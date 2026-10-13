// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <QtCore/QFileInfo>
#include <QtGui/QImageReader>
#include <QtGui/QPixmap>
#include <QtConcurrent/QtConcurrentRun>
#include <suzuri/entity/common.hpp>
#include <suzuri/utility/config_utils.hpp>
#include <suzuri/utility/thumbnail.hpp>
#include "utility/sizing.hpp"
#include "thumbnailmanager.hpp"

namespace rin
{
    thumbnail_manager::thumbnail_manager(const std::filesystem::path& db_path, QObject* parent) :
        QThread(parent), m_db(db_path, true)
    {
        start(QThread::Priority::LowPriority);
        m_working = false;
        m_do_interrupt.clear();
    }

    thumbnail_manager::~thumbnail_manager()
    {
        m_request_abort();
        wait();
    }

    int thumbnail_manager::submit_batch(const std::vector<thumbnail_descriptor>& batch)
    {
        QMutexLocker locker(&m_mutex);

        const int key = ++m_seed;
        m_queue.push_front(key);
        
        if (m_batches.contains(key))
        {
            m_batches[key].descriptors = batch;
            m_batches[key].position = 0;
        }
        else
        {
            m_batches.emplace(key, job_batch{
                .descriptors = batch,
                .position = 0
            });
        }
        m_have_job.wakeAll();
        m_continue.wakeAll();
        return key;
    }

    void thumbnail_manager::clear()
    {
        QMutexLocker locker(&m_mutex);
        m_interrupt_and_wait_for_job_thread();
        m_batches.clear();
        m_queue.clear();
        m_seed = 0;
        m_continue.notify_all();
    }

    void thumbnail_manager::request_abort()
    {
        m_request_abort();
    }

    void thumbnail_manager::set_database(const std::filesystem::path& db_path)
    {
        QMutexLocker locker(&m_mutex);
        m_interrupt_and_wait_for_job_thread();
        m_db.set_database(db_path);
    }

    void thumbnail_manager::run()
    {
        setTerminationEnabled(false);
        forever
        {
            QMutexLocker locker(&m_mutex);

            while (!isInterruptionRequested() && m_queue.empty())
            {
                m_have_job.wait(&m_mutex);
            }

            if (isInterruptionRequested())
            { return; }

            // do work
            const int key = std::as_const(m_queue.front());
            
            m_working = true;
            m_mutex.unlock();
            setTerminationEnabled(true);
            
            const bool batch_done = m_exec_batch(m_batches[key]);
            
            setTerminationEnabled(false);
            m_mutex.lock();
            
            if (batch_done)
            { m_remove(key); }

            m_working = false;
            m_no_work_being_performed.notify_all();
            
            if (!batch_done || m_do_interrupt.test())
            {
                m_do_interrupt.clear();
                m_continue.wait(&m_mutex);
            }
        }
    }

    void thumbnail_manager::m_interrupt_and_wait_for_job_thread()
    {
        m_do_interrupt.test_and_set();

        if (m_working)
        { m_no_work_being_performed.wait(&m_mutex); }
    }

    void thumbnail_manager::m_remove(const int key)
    {
        m_batches.erase(key);
        auto end = std::remove(m_queue.begin(), m_queue.end(), key);
        m_queue.erase(end, m_queue.end());
    }

    void thumbnail_manager::m_request_abort()
    {
        requestInterruption();
        QMutexLocker locker(&m_mutex);
        m_do_interrupt.test_and_set();
        m_have_job.wakeAll();
        m_continue.wakeAll();
    }

    // for file entities only
    bool thumbnail_manager::m_is_supported_entity(const std::filesystem::path& path) const
    {
        const auto exts = sz::supported_extensions();
        return exts.contains(path.extension().string().substr(1));
    }

    // TODO
    bool thumbnail_manager::m_is_supported_entity(QString entity_uri) const
    {
        if (entity_uri.startsWith("file://", Qt::CaseSensitivity::CaseInsensitive))
        {
            return m_is_supported_entity(std::filesystem::path(entity_uri.replace("file://", "", Qt::CaseInsensitive).toLower().toStdString()));
        }
        return false;
    }

    QString thumbnail_manager::m_thumbnail_cache_path(const thumbnail_descriptor& d) const
    {
        return QString::fromStdString(sz::thumbnail_cache_path(sz::entity_uri(d.id.toStdString(), d.entity_type), d.target_size).string());
    }

    QSize thumbnail_manager::m_thumbnail_size(const thumbnail_descriptor& d) const
    {
        const int px = sz::from_thumbnail_target_size(d.target_size);
        return rin::fit_under(QSize(px, px), d.original_size);
    }

    QPixmap thumbnail_manager::m_create_thumbnail(const thumbnail_descriptor& d)
    {
        const bool is_file = d.entity_type == sz::entity_type::File;
        QPixmap thumb;
        
        if (is_file)
        {
            QImageReader reader(d.id);
            reader.setScaledSize(m_thumbnail_size(d));
            thumb = QPixmap::fromImageReader(&reader);
        }
        // TODO

        return thumb;
    }

    // d.thumbnail not expected to be populated
    bool thumbnail_manager::m_thumbnail_is_up_to_date(const thumbnail_descriptor& d, const QString& thumbnail_path)
    {
        using enum sz::entity_type;
        if (QImage image(thumbnail_path); !image.isNull())
        {
            if (const auto keys = image.textKeys(); keys.contains("Thumb::MTime"))
            {
                const QString& id = d.id;
                const qint64 recorded_mtime = image.text("Thumb::MTime").toLongLong();
                if (d.entity_type == File && keys.contains("Thumb::Size"))
                {
                    const QFileInfo info(id);
                    const qint64 mtime = info.lastModified().toSecsSinceEpoch();
                    const qint64 fsize = info.size();
                    return (mtime == recorded_mtime) && (fsize == image.text("Thumb::Size").toLongLong());
                }
                else if (auto info = m_db.entity_info(id.toStdString()); info.id.size())
                {
                    return static_cast<qint64>(info.modtime) == recorded_mtime;
                }
            }
        }
        return false;
    }

    bool thumbnail_manager::m_write_thumbnail(const thumbnail_descriptor& d, const QString& thumbnail_path)
    {
        static constexpr const std::string_view software = "rin";

        // TODO: eventually we will need to check a platform-specific id (paths formatted for the current platform)
        if (d.id.startsWith(QString::fromStdString(sz::thumbnail_cache_path().string())))
        { return false; }

        QImage image = d.thumbnail.toImage();
        qint64 mtime = 0;
        qint64 fsize = 0;
        QString description;
        
        const std::string eid = d.id.toStdString();
        const auto info = m_db.entity_info(eid);
        const bool is_indexed = !info.id.empty();
        
        if (d.entity_type == sz::entity_type::File)
        {
            QFileInfo finfo(d.id);
            mtime = finfo.lastModified().toSecsSinceEpoch();
            fsize = finfo.size();
        }
        else
        { mtime = is_indexed ? static_cast<qint64>(info.modtime) : 0; }
        
        if (mtime)
        {
            description = QString::fromStdString(info.description);
        
            image.setText("Thumb::URI", QString::fromStdString(sz::entity_uri(eid, d.entity_type)));
            image.setText("Thumb::MTime", QString::fromStdString(std::to_string(mtime)));
            
            if (fsize)
            { image.setText("Thumb::Size", QString::fromStdString(std::to_string(fsize))); }

            image.setText("Description", description);
            image.setText("Software", software.data());
            image.setText("Thumb::Image::Width", QString::fromStdString(std::to_string(d.original_size.width())));
            image.setText("Thumb::Image::Height", QString::fromStdString(std::to_string(d.original_size.height())));

            if (const QString tmp_path = thumbnail_path + "_rin-tmp.png"; image.save(tmp_path))
            { return QFile::rename(tmp_path, thumbnail_path); }
        }
        return false;
    }

    bool thumbnail_manager::m_exec_batch(job_batch& batch)
    {
        using enum entity_attribute_type;
        for (thumbnail_descriptor& d : batch.descriptors)
        {
            if (m_do_interrupt.test())
            { break; }
            
            if (m_is_supported_entity(QString::fromStdString(sz::entity_uri(d.id.toStdString(), d.entity_type))))
            {
                if (const QString thumbnail_path = m_thumbnail_cache_path(d); m_thumbnail_is_up_to_date(d, thumbnail_path))
                {
                    d.thumbnail = QPixmap(thumbnail_path);
                }
                else
                {
                    d.thumbnail = m_create_thumbnail(d);
                    if (const QSize s = m_thumbnail_size(d); s.width() > 64 || s.height() > 64)
                    {
                        // save the generated thumbnail to the on-disk cache
                        if (m_write_thumbnail(d, thumbnail_path))
                        { qDebug() << "thumbnail manager: thumbnail saved to cache:" << thumbnail_path; }
                        else
                        { qDebug() << "thumbnail manager: failed to write to cache:" << thumbnail_path; }
                    }
                }
                emit thumbnail_available(d);
            }
            ++batch.position;
        }
        return batch.position >= batch.descriptors.size();
    }
}
