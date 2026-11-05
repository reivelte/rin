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
#include "tree.hpp"

namespace rin
{
    namespace detail
    {
        static constexpr const std::string_view s_query_cmd_taglist = "!taglist:";

        /* ******************** 
        Begin definitions for data sources
        ******************** */
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
        /* ******************** 
        End definitions for data sources
        ******************** */

        template <typename T>
        struct data_source_container {};

        /* ******************** 
        Begin definitions for data source containers
        
        These include defining how to advance the position in and how to retrieve
        data from sources. Template specializations are used to avoid the
        overhead of inheritance.
        ******************** */
        template <>
        struct data_source_container<database_source>
        {
            database_source src;
            
            data_source_container(database_source&& s) : src(std::move(s)) {}
            
            inline operator database_source&() { return src; }
            inline data_source_container<database_source>& operator++()
            {
                ++src;
                return *this;
            }

            inline bool has_data() const { return src.good(); }
            inline QString id_from_source() const { return QString::fromStdString(src.get<std::string>()); }
        };

        template <>
        struct data_source_container<filesystem_source>
        {
            filesystem_source src;

            data_source_container(filesystem_source&& s) : src(std::move(s)) {}
            
            inline operator filesystem_source&() { return src; }
            inline data_source_container<filesystem_source>& operator++()
            {
                ++src.it;
                return *this;
            }

            inline bool has_data() const { return src.it != src.dir.end(); }
            inline QString id_from_source() const { return src.it->absoluteFilePath(); }
        };

        template <>
        struct data_source_container<concept_source>
        {
            concept_source src;

            data_source_container(concept_source&& s) : src(std::move(s)) {}
            data_source_container(const concept_source& s) : src(s) {}
            data_source_container(concept_source& s) : src(s) {}
            
            inline operator concept_source&() { return src; }
            inline data_source_container<concept_source>& operator++()
            {
                ++src.position;
                return *this;
            }

            inline bool has_data() const { return std::cmp_less(src.position, src.urls.size()); }
            inline QString id_from_source() const
            {
                // TODO: other url types
                return src.urls[src.position].toLocalFile(); 
            }
        };

        /* ******************** 
        End definitions for data source containers
        ******************** */

        template <typename T>
        concept is_data_source = 
            std::same_as<T, database_source> ||
            std::same_as<T, filesystem_source> ||
            std::same_as<T, concept_source>;

        typedef std::variant<
            data_source_container<database_source>,
            data_source_container<filesystem_source>,
            data_source_container<concept_source>
        > data_source_variant;

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
        class data_source : public detail::data_source_variant
        {
            using base = detail::data_source_variant;
            using base::base;
            using enum query_descriptor::query_type;

            public:
            explicit data_source(detail::database_source&& src) : base(std::move(src)), m_type(Indexed)
            {
                // move the query object to the first valid row, if there is one
                auto& data = std::get<detail::data_source_container<detail::database_source>>(*this);
                ++data.src;
            }
            data_source(const detail::database_source&) = delete;
            data_source(detail::database_source&) = delete;
            
            explicit data_source(detail::filesystem_source&& src) : base(std::move(src)), m_type(Filesystem)
            {
                auto& data = std::get<detail::data_source_container<detail::filesystem_source>>(*this);
                data.src.it = data.src.dir.begin();
            }
            data_source(const detail::filesystem_source&) = delete;
            data_source(detail::filesystem_source&) = delete;

            data_source(const detail::concept_source& src) : base(src), m_type(Concept) { }
            data_source(detail::concept_source&& src) : base(std::move(src)), m_type(Concept) { }
            data_source(detail::concept_source& src) : base(src), m_type(Concept) { }

            operator detail::database_source&() { return std::get<detail::data_source_container<detail::database_source>>(*this).src; }
            operator detail::filesystem_source&() { return std::get<detail::data_source_container<detail::filesystem_source>>(*this).src; }
            operator detail::concept_source&() { return std::get<detail::data_source_container<detail::concept_source>>(*this).src; }

            constexpr query_descriptor::query_type type() const noexcept { return m_type; }

            template <detail::is_data_source Source>
            inline bool has_data() const
            {
                return std::get<detail::data_source_container<Source>>(*this).has_data();
            }

            inline bool has_data() const
            {
                using namespace rin::detail;
                switch (m_type)
                {
                case Filesystem: return std::get<data_source_container<filesystem_source>>(*this).has_data();
                case Indexed: return std::get<data_source_container<database_source>>(*this).has_data();
                case Concept: return std::get<data_source_container<concept_source>>(*this).has_data();
                default: break;
                }
                return false;
            }

            template <detail::is_data_source Source>
            inline QString id_from_source() const
            {
                return std::get<detail::data_source_container<Source>>(*this).id_from_source();
            }

            private:
            query_descriptor::query_type m_type;
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

        inline reflexive_entity m_make_entity(const QString& parent_id, const QString& id, std::optional<QFileInfo> fsinfo = std::nullopt);
        inline void m_fill_extended_item_attributes(reflexive_entity& e);
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
        void m_iterate_source(entity_data_manager::data_source& data, entity_tree_node& node);

        template <detail::is_data_source Source>
        bool m_push_result(reflexive_entity&& x, QElapsedTimer& t, detail::data_source_container<Source>& src, entity_tree_node& node, int64_t& retrieved_this_time, int& start);

        private:
        using enum entity_attribute_type;
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

    template <detail::is_data_source Source>
    inline void entity_data_manager::m_iterate_source(entity_data_manager::data_source& x, entity_tree_node& node)
    {
        using namespace rin::detail;
        using enum query_descriptor::query_type;
        QElapsedTimer timer;
        int64_t retrieved = 0;
        int start = node.size();
        // timer.start();

        auto& src = std::get<data_source_container<Source>>(x);
        while (src.has_data())
        {
            std::optional<QFileInfo> info = std::nullopt;
            if constexpr (std::same_as<Source, filesystem_source>)
            {
                filesystem_source& raw = x;
                info = raw.it->fileInfo();
            }
            reflexive_entity e = m_make_entity(node.descriptor.text, src.id_from_source(), info);
            
            if (m_push_result(std::move(e), timer, src, node, retrieved, start))
            { break; }
        }
    }

    template <detail::is_data_source Source>
    inline bool entity_data_manager::m_push_result(reflexive_entity&& e, QElapsedTimer& t, detail::data_source_container<Source>& src, entity_tree_node& node, int64_t& retrieved_this_time, int& start)
    {
        using enum entity_tree_node_state;
        using enum entity_attribute_type;
        
        const QString name = e.attribute<QString>(Name);
        // const bool time_is_up = t.hasExpired(static_cast<qint64>(m_deadline));
        const bool time_is_up = false;
        const bool do_interrupt = m_do_interrupt.test();
        bool ret = false;

        if (const QString ft = node.descriptor.full_text(); m_invalidated.contains(ft) && m_invalidated[ft].contains(name))
        { m_invalidated[ft].erase(name); }

        e.set_parent_key(node.key);
        m_adjust_count(node.descriptor, e, true); // descriptors are always redone for invalidates
        
        if (!node.contains(name))
        { node.insert(std::move(e)); }
        
        ++retrieved_this_time;
        ++src;
        ret = !src.has_data();
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

} // namespace rin
