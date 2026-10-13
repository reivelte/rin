// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <unordered_set>
#include <vector>
#include <deque>
#include <atomic>
#include <QtCore/QWaitCondition>
#include <QtCore/QMutexLocker>
#include <QtCore/QMutex>
#include <QtCore/QString>
#include <QtCore/QSize>
#include <QtCore/QThread>
#include <QtCore/QModelIndex>
#include <suzuri/entity/database.hpp>
#include "common.hpp"
#include "entity.hpp"

namespace rin
{
    // TODO: read from memory cache

    class thumbnail_manager : public QThread
    {
        Q_OBJECT

        public:
        thumbnail_manager(const std::filesystem::path& db_path, QObject* parent = nullptr);
        ~thumbnail_manager();

        int submit_batch(const std::vector<thumbnail_descriptor>& batch);
        void clear();
        void request_abort();
        void set_database(const std::filesystem::path& db_path);

        signals:
        void thumbnail_available(const thumbnail_descriptor& descriptor);

        protected:
        struct job_batch
        {
            std::vector<thumbnail_descriptor> descriptors;
            size_t position;
        };

        protected:
        void run() override;

        private:
        void m_interrupt_and_wait_for_job_thread();
        void m_remove(const int key);
        void m_request_abort();

        bool m_is_supported_entity(const std::filesystem::path& path) const;
        bool m_is_supported_entity(QString entity_uri) const;
        QString m_thumbnail_cache_path(const thumbnail_descriptor& d) const;
        QSize m_thumbnail_size(const thumbnail_descriptor& d) const;
        QPixmap m_create_thumbnail(const thumbnail_descriptor& d);
        bool m_thumbnail_is_up_to_date(const thumbnail_descriptor& d, const QString& thumbnail_path);
        bool m_write_thumbnail(const thumbnail_descriptor& d, const QString& thumbnail_path);
        
        bool m_exec_batch(job_batch& batch);

        private:
        mutable QMutex m_mutex;
        std::atomic_flag m_do_interrupt;
        std::atomic<bool> m_working;
        QWaitCondition m_have_job;
        QWaitCondition m_continue;
        QWaitCondition m_no_work_being_performed;
        
        sz::entity_database m_db; // in read_only mode
        std::deque<int> m_queue;
        std::unordered_map<int, job_batch> m_batches;
        int m_seed;

    };

}
