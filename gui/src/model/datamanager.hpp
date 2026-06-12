// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <deque>
#include <atomic>
#include <QtCore/QThread>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>
#include <QtCore/QString>
#include <QtCore/QSet>
#include <QtCore/QHash>
#include <QtCore/QFileSystemWatcher>
#include <QtCore/QFileInfo>
#include <QtCore/QDirListing>
#include <QtCore/QDirIterator>
#include <QtCore/QElapsedTimer>
#include <QtCore/QMimeDatabase>
#include <QtCore/QSize>
#include <QtWidgets/QFileIconProvider>
#include <QtGui/QImageReader>
#include <suzuri/entity/database.hpp>
#include "thumbnailmanager.hpp"
#include "tree.hpp"

namespace rin
{
    namespace detail
    {
        static constexpr std::string_view s_query_cmd_tagsof = "!tagsof:";

        template <typename T>
        struct deduce_from {};

        typedef sz::sqlite::database_query database_source;    
        
        struct filesystem_source
        {
            QDirListing dir;
            QDirListing::const_iterator it;
        };

        struct concept_source
        {
            QList<QUrl> urls{};
            int position{};
        };

        template <>
        struct deduce_from<QFileInfo>
        {
            typedef filesystem_source ValueType;
        };

        template <>
        struct deduce_from<std::string>
        {
            typedef database_source ValueType;
        };

        template <typename T>
        concept is_data_source = 
            std::same_as<T, database_source> ||
            std::same_as<T, filesystem_source> ||
            std::same_as<T, concept_source>;

    } // namespace rin::detail

    class entity_data_manager : public QThread
    {
        Q_OBJECT
        
        public:
        entity_data_manager(const std::filesystem::path& database_path, QObject* parent = nullptr);
        ~entity_data_manager();

        void watch_path(const QString& path);
        void unwatch_path(const QString& path);
        void set_watcher_enabled(bool enabled);
        void set_batchsize(size_t size);
        void set_deadline(int ms);
        void set_database(const std::filesystem::path& path);
        void clear();
        void request_abort();

        int query(const QString& short_text, entity_tree& tree, int index, int parent, std::optional<entity_attribute_type> sort_key = std::nullopt, std::optional<Qt::SortOrder> sort_order = std::nullopt);
        int query(const QString& short_text, entity_tree& tree, const QList<QUrl>& urls, int index, int parent);
        void query(const entity_tree_node& node);
        void sort(entity_tree_node& node, const std::unordered_map<int, int>& keys);
        void fetch(const QString& query_string, const std::vector<entity_model_url>& urls, fetch_action action, bool is_update = false);
        int tag(const std::string& entity_id, const std::vector<std::string>& tags); // synchronous
        index_descriptor tag(const QString& entity_id, const sz::metadata::parameters& args); // async
        void continue_query();
        void continue_query(const query_descriptor& qd);
        void cancel_query(const query_descriptor& qd);
        void cancel_query(const QString& text, query_descriptor::query_mode mode);

        signals:
        void data_available(const entity_tree_node& node, int start, int end);
        void data_available(const query_descriptor& qd, const std::vector<reflexive_entity>& data);
        void data_sorted(const entity_tree_node& node, const std::vector<node_index_change>& changed_key_indexes);
        void data_removed(const query_descriptor& qd, const std::vector<std::tuple<QString, int>>& removed);
        void update_available(const query_descriptor& qd, std::vector<reflexive_entity> data);
        void update_available(const entity_tree_node& node, int start, int end);
        void query_needs_update(const QString& query_string);
        void query_progressed(query_descriptor qd, QList<std::tuple<QString, int>> items_processed); // tuple of <item, sub-items processed>
        void query_failed(query_descriptor qd, QList<std::tuple<QString, QString>> failed_items); // tuple of <item, error message>
        void query_done(query_descriptor qd);
        void fetch_progressed(fetch_descriptor fd, std::vector<entity_model_url> urls_processed);
        void fetch_failed(fetch_descriptor fd, std::vector<entity_model_url> items);

        protected:
        bool event(QEvent* e) override;

        private: // internal data structures to make life easier
        class data_source : public std::variant<
            detail::filesystem_source, detail::database_source, detail::concept_source
        >
        {
            using base = std::variant<detail::filesystem_source, detail::database_source, detail::concept_source>;
            using base::base;
            using enum query_descriptor::query_type;

            public:
            explicit data_source(detail::database_source&& src) : base(std::move(src)), m_type(Indexed) { }
            data_source(const detail::database_source&) = delete;
            data_source(detail::database_source&) = delete;
            
            explicit data_source(detail::filesystem_source&& src) : base(std::move(src)), m_type(Filesystem) { }
            data_source(const detail::filesystem_source&) = delete;
            data_source(detail::filesystem_source&) = delete;

            data_source(const detail::concept_source& src) : base(src), m_type(Concept) { }
            data_source(detail::concept_source&& src) : base(std::move(src)), m_type(Concept) { }
            data_source(detail::concept_source& src) : base(src), m_type(Concept) { }

            constexpr query_descriptor::query_type type() const noexcept { return m_type; }
            inline bool at_end() const;

            private:
            query_descriptor::query_type m_type;
        };

        class descriptor : public std::variant<query_descriptor, fetch_descriptor>
        {
            using base = std::variant<query_descriptor, fetch_descriptor>;
            using base::base;
            using enum query_descriptor::query_mode;

            public:
            explicit descriptor(query_descriptor&& qd) : base(std::move(qd)), m_mode(Read) {}
            explicit descriptor(fetch_descriptor&& fd) : base(std::move(fd)), m_mode(Fetch) {}

            constexpr query_descriptor::query_mode mode() const noexcept { return m_mode; }

            private:
            query_descriptor::query_mode m_mode;
        };
        
        void run() override;
        void m_request_abort();
        void m_interrupt_and_wait_for_job_thread();
        inline void m_tell_job_thread_to_continue();

        inline bool m_is_in_queue(const query_descriptor& qd);
        inline void m_erase_job_data(const QString& text, query_descriptor::query_mode mode);
        inline void m_remove_from_queue(const query_descriptor& qd);
        inline void m_remove_from_queue(const QString& text, query_descriptor::query_mode mode);
        int64_t m_query_size(const QString& text);
        bool m_query_size(query_descriptor& qd, entity_data_manager::data_source& source, bool emit_signal);
        query_descriptor::query_type m_query_type(const QString& short_text) const;
        [[nodiscard]] entity_data_manager::data_source m_create_source(const QString& short_text, query_descriptor::query_type source_type);
        [[nodiscard]] entity_data_manager::data_source m_create_source(const QList<QUrl>& urls);
        [[nodiscard]] query_descriptor m_create_query_descriptor(const QString& short_text, const entity_data_manager::data_source& source) const;
        [[nodiscard]] query_descriptor m_create_query_descriptor(const QString& entity_id, const sz::metadata::parameters& args) const;
        query_descriptor m_create_fetch_query(const QString& text, const std::vector<entity_model_url>& urls, fetch_action action, bool update_only);

        inline reflexive_entity m_make_item(const QString& parent_id, const QString& id, const QFileInfo& fsinfo, sz::entity_type type, bool is_indexed, bool fill_extended_attributes);
        inline void m_fill_extended_item_attributes(reflexive_entity& e, const QString& id, bool is_file, bool is_indexed);
        bool m_run_read_query(entity_tree_node& node);
        bool m_run_extended_read_query(entity_tree_node& node, int& position);
        bool m_run_sort_query(entity_tree_node& node, const std::unordered_map<int, int>& keys);
        bool m_run_fetch_query(fetch_descriptor& fd);
        bool m_run_indexed_write_query(index_descriptor& ixd);
        bool m_exec(QMutex& mutex, const std::tuple<QString, query_descriptor::query_mode> job); // dont pass by reference

        void m_adjust_count(query_descriptor& qd, const reflexive_entity& e, bool increment);
        reflexive_entity m_rename_file_entity(const QString& old_id, const QString& new_id, bool is_indexed);
        reflexive_entity m_copy_file_entity(const QString& src_id, const QString& dst_id, bool is_indexed);
        reflexive_entity m_link_file_entity(const QString& id, const QString& target, bool is_indexed);
        bool m_remove_file_entity(const QString& id, bool is_indexed);
        bool m_recycle_file_entity(const QString& id, bool is_indexed);

        QFileSystemWatcher* m_create_watcher();

        template <detail::is_data_source Source>
        void m_iterate_source(Source& src, entity_tree_node& node);

        template <detail::is_data_source Source> 
        bool m_push_result(reflexive_entity&& x, QElapsedTimer& t, Source& source, entity_tree_node& node, int64_t& retrieved_this_time, int& start);

        template <detail::is_data_source Source>
        reflexive_entity m_item_from_source(const query_descriptor& qd, const Source& src, bool is_tagsof_query = false);

        private:
        // accessed by multiple threads:
        mutable QMutex m_mutex;
        std::atomic_flag m_do_interrupt; // callers set this when they want to request an interruption to query processing
        
        // callers set this when telling dataman to continue processing the current (or next) job
        // the job thread will clear this if it finds it set
        std::atomic_flag m_should_continue;
        
        std::atomic<bool> m_working; // set in m_exec() when job thread is about to start processing a query
        QWaitCondition m_have_query; // manager waits for a query to be present
        QWaitCondition m_continue_query; // manager waits to be told to continue a query
        QWaitCondition m_no_work_being_performed; // model waits for any executing query function to return
        std::deque<std::tuple<QString, query_descriptor::query_mode>> m_queries;
        std::unordered_map<QString, index_descriptor> m_indexed_writes; // for tagging, entity links, database writes, etc.
        std::unordered_map<QString, fetch_descriptor> m_fetches;
        std::unordered_map<QString, data_source> m_sources;
        std::unordered_map<QString, std::unordered_set<QString>> m_invalidated;
        std::unordered_map<QString, entity_tree_node> m_nodes;
        std::unordered_map<QString, entity_tree_node> m_extended_reads;
        std::unordered_map<QString, int> m_extended_read_positions;
        std::unordered_map<QString, entity_tree_node> m_pending_sort;
        std::unordered_map<QString, std::unordered_map<int, int>> m_pending_sort_keys;
        sz::entity_database m_database; // only when clear() is called
        QFileSystemWatcher* m_watcher;
        
        // only accessed by this thread:
        QFileIconProvider m_icon_provider;
        QMimeDatabase m_mimedb;

        // only set by model, only read in this->thread (that is, the job thread never modifies these variables)
        size_t m_batchsize;
        int m_deadline; // in milliseconds
        bool m_watcher_enabled;
    };

    // template <detail::is_data_source Source>
    // inline constexpr bool entity_data_manager::data_source::next()
    // {
    //     if constexpr (std::same_as<Source, detail::database_source>)
    //     {
    //         auto& db_src = std::get<detail::database_source>(*this);
    //         ++db_src;
    //         return db_src.good();
    //     }
    //     else
    //     {
    //         auto& dir_src = std::get<detail::filesystem_source>(*this);
    //         ++dir_src.it;
    //         return dir_src.it != dir_src.dir.end();
    //     }
    // }

    // template <detail::is_data_source Source>
    // inline constexpr bool entity_data_manager::data_source::good() const
    // {
    //     if constexpr (std::same_as<Source, detail::database_source>)
    //     {
    //         const auto& db_src = std::get<detail::database_source>(*this);
    //         return db_src.good();
    //     }
    //     else
    //     {
    //         const auto& x = std::get<detail::filesystem_source>(*this);
    //         return x.it != x.dir.end();
    //     }
    // }

    // template <typename T, typename Source> requires (std::same_as<T, QFileInfo> || std::same_as<T, std::string>)
    // inline constexpr T entity_data_manager::data_source::get() const
    // {
    //     if constexpr (std::same_as<Source, detail::filesystem_source>)
    //     {
    //         const auto& dir_src = std::get<detail::filesystem_source>(*this);
    //         return dir_src.it->fileInfo();
    //     }
    //     else
    //     {
    //         const auto& db_src = std::get<detail::database_source>(*this);
    //         return db_src.column<T>();
    //     }
    // }

    // template <detail::is_data_source Source>
    // inline constexpr bool entity_data_manager::data_source::at_start() const
    // {
    //     if constexpr (std::same_as<Source, detail::database_source>)
    //     {
    //         const auto& db_src = std::get<detail::database_source>(*this);
    //         return db_src.status() == sz::sqlite::status_code::OK;
    //     }
    //     // this function is not used for filesystem_source
    //     return true;
    // }

    inline bool entity_data_manager::data_source::at_end() const
    {
        if (m_type == Indexed)
        {
            const auto& db_src = std::get<detail::database_source>(*this);
            return db_src.status() == sz::sqlite::status_code::Done;
        }
        else if (m_type == Filesystem)
        {
            const auto& x = std::get<detail::filesystem_source>(*this);
            return x.it == x.dir.end();
        }
        return true;
    }

    template <detail::is_data_source Source>
    inline void entity_data_manager::m_iterate_source(Source& src, entity_tree_node& node)
    {
        using namespace rin::detail;
        QElapsedTimer timer;
        int64_t retrieved = 0;
        int start = node.size();

        // timer.start();
        if constexpr (std::same_as<Source, filesystem_source>)
        {
            auto& fs = static_cast<filesystem_source&>(src);
            while (fs.it != fs.dir.end())
            {
                if (m_push_result(m_item_from_source(node.descriptor, fs, false), timer, fs, node, retrieved, start))
                { break; }
            }
        }
        else if constexpr (std::same_as<Source, database_source>)
        {
            // TODO: handle cases where the database is busy/locked
            auto& db_src = static_cast<database_source&>(src);
            const bool tagsof = node.descriptor.text.startsWith(s_query_cmd_tagsof.data(), Qt::CaseInsensitive);
            
            if (db_src.status() == sz::sqlite::status_code::OK)
            { ++db_src; } // the very beginning of a db query object does not point to a valid row, lets advance now

            while (db_src.good())
            {
                if (m_push_result(m_item_from_source(node.descriptor, db_src, tagsof), timer, db_src, node, retrieved, start))
                { break; }
            }
        }
        else if constexpr (std::same_as<Source, concept_source>)
        {
            auto& data = static_cast<concept_source&>(src);

            for ( ; std::cmp_less(data.position, data.urls.size()); )
            {
                if (m_push_result(m_item_from_source(node.descriptor, data, false), timer, data, node, retrieved, start))
                { break; }
            }
        }
    }

    template <detail::is_data_source Source>
    inline bool entity_data_manager::m_push_result(reflexive_entity &&x, QElapsedTimer &t, Source &source, entity_tree_node &node, int64_t &retrieved_this_time, int &start)
    {
        using enum entity_tree_node_state;
        using enum entity_attribute_type;
        // const bool time_is_up = t.hasExpired(static_cast<qint64>(m_deadline));
        const bool time_is_up = false;
        const bool do_interrupt = m_do_interrupt.test();
        
        const QString name = x.attribute<QString>(Name);

        if (const QString ft = node.descriptor.full_text(); m_invalidated.contains(ft) && m_invalidated[ft].contains(name))
        { m_invalidated[ft].erase(name); }

        x.set_parent_key(node.key);
        m_adjust_count(node.descriptor, x, true); // descriptors are always redone for invalidates
        
        if (!node.contains(name))
        {
            node.insert(std::move(x));
        }
        
        ++retrieved_this_time;
        
        bool ret = false;
        if constexpr (std::same_as<Source, detail::database_source>)
        {
            auto& db_src = static_cast<detail::database_source&>(source);
            ++db_src;
            ret = !db_src.good();
        }
        else if constexpr (std::same_as<Source, detail::filesystem_source>)
        {
            auto& dir_src = static_cast<detail::filesystem_source&>(source);
            ++dir_src.it;
            ret = dir_src.it == dir_src.dir.end();
        }
        else if constexpr(std::same_as<Source, detail::concept_source>)
        {
            auto& concept_src = static_cast<detail::concept_source&>(source);
            ++concept_src.position;
            ret = concept_src.position >= concept_src.urls.size();
        }
        ret = (!ret ? (do_interrupt || time_is_up || std::cmp_greater_equal(retrieved_this_time, m_batchsize)) : ret);
        if (ret)
        {
            // if the node has a set sort preference, lets sort it now
            if (node.descriptor.sort_key && !(node.state == Updating) && node.size() > 1)
            { rin::sort_entities(node); }

            node.descriptor.processed += retrieved_this_time;
            const int end = node.size() - 1;
            
            if (start <= end) // otherwise no new data was inserted. (this is applicable mainly to invalidates)
            { emit data_available(node, start, end); }
            
            start = end + 1;
            if (time_is_up && (!do_interrupt) && std::cmp_less(retrieved_this_time, m_batchsize))
            {
                // we want to continue iterating till we have reached batchsize amount of items retrieved
                // if ret was true because we reached the end of our source,
                // the outer while loop will stop executing after we return
                ret = false;
                t.restart();
            }
        }
        return ret;
    }

    template <detail::is_data_source Source>
    inline reflexive_entity entity_data_manager::m_item_from_source(const query_descriptor& qd, const Source& src, bool is_tagsof_query)
    {
        using namespace rin::detail;
        // in the call to m_make_item(), we pass false for 'fill_extended_attributes' here in all cases, so the value of is_indexed doesn't matter
        if constexpr (std::same_as<Source, filesystem_source>)
        {
            Q_UNUSED(is_tagsof_query);
            const auto& fs_src = static_cast<const filesystem_source&>(src);
            const QFileInfo fsinfo = fs_src.it->fileInfo();
            const QString id = fsinfo.absoluteFilePath();
            return m_make_item(qd.text, id, fsinfo, sz::entity_type::File, false, false);
        }
        else if constexpr (std::same_as<Source, detail::database_source>)
        {
            const auto& db_src = static_cast<const database_source&>(src);
            const QString id_qstr = QString::fromStdString(db_src.get<std::string>());
            const QFileInfo fsinfo = is_tagsof_query ? QFileInfo() : QFileInfo(id_qstr);
            const bool is_file = !is_tagsof_query;
            return m_make_item(qd.text, id_qstr, fsinfo, is_file ? sz::entity_type::File : sz::entity_type::Tag, true, false);
        }
        else if constexpr (std::same_as<Source, concept_source>)
        {
            const auto& data = static_cast<const concept_source&>(src);
            const auto& url = data.urls.at(data.position);
            
            // TODO: other url types. for now we assume every url points to a filesystem location
            const QString path = url.toLocalFile();
            const QFileInfo info(path);
            return m_make_item(qd.text, path, info, sz::entity_type::File, false, false);
        }
    }

} // namespace rin
