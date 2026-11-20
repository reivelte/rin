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
#include <QtCore/QFileSystemWatcher>
#include <QtCore/QFileInfo>
#include <QtCore/QElapsedTimer>
#include <QtCore/QMimeDatabase>
#include <QtCore/QSize>
#include <QtWidgets/QFileIconProvider>
#include <suzuri/entity/database.hpp>
#include "tree.hpp"
#include "descriptor.hpp"
#include "data_source.hpp"

namespace rin
{
    namespace detail
    {
        static constexpr std::string_view s_query_cmd_taglist = "!taglist:";
        static constexpr std::string_view s_query_cmd_simto = "!taglist/similar_to";
    }

    namespace query_operator
    {
        enum : i8
        {
            None = 0,
            Read = 0x1,
            Read_Extended = 0x2,
            Sort = 0x4,
            Fetch = 0x6,
            Write = 0x8
        };
    }

    typedef i8 query_operator_flags;
    enum class enqueue_position : i8 { Front, Back };
    
    struct data_node_update
    {
        data_node node;
        int start;
        int end;
        query_operator_flags flags;
        std::optional<data_node_layout_changes> indexes_changed_from_sort;
        std::optional<data_node_layout_changes> indexes_changed_from_remove;
        std::optional<data_node_index_removal_list> indexes_removed;
    };

    class entity_data_manager : public QThread
    {
        Q_OBJECT
        
        public: // constructors/operators
        entity_data_manager(const std::filesystem::path& database_path, entity_model* model, QObject* parent = nullptr);
        ~entity_data_manager();

        public: // setters
        void set_watcher_enabled(bool enabled);
        void set_watched(const QString& uri, bool watched);
        void set_batchsize(int size);
        void set_deadline(int ms);
        void set_database(const std::filesystem::path& path);

        public: // mutators
        void clear();
        void request_abort();
        void enqueue(data_node dst, data_node opt, query_operator_flags op, enqueue_position pos = enqueue_position::Front);
        void dequeue(data_node_key key, query_operator_flags op = query_operator::None);

        signals:
        void data_available(data_node_update update);
        void data_changed(QString uri);
        void job_complete(data_node_key key, query_operator_flags op);

        protected:
        virtual data_source create_source(const reflexive_descriptor& d);
        virtual reflexive_entity create_entity(const data_node& data, QString parent_id, QString id, query_operator_flags flags);
        virtual bool read(data_node& dst, data_node& opt, data_source& source, query_operator_flags flags);
        
        protected: // reimplemented functions
        bool event(QEvent* e) override;
        void run() override;

        private:
        enum class manager_state { Idle = 0, Working };
        enum class continue_condition
        {
            None = 0,   // proceed as normal
            Discard,    // discard sources and results for everything in the discard list
            Discard_All, // assume all job data to be invalid until the next job is executed
            Reset // reinitialize all internal structures, implies Discard_All
        };
        
        struct job_frame
        {
            data_node_key key = 0;
            query_operator_flags op = query_operator::None;
            bool done = false;
        };

        struct job_data
        {
            data_node dst;
            data_node opt;

            void set_locked(bool locked)
            {
                dst->locked = locked;
                if (opt) {
                    opt->locked = locked;
                }
            }
            void copy(const data_node& d, const data_node& o)
            {
                dst = rin::make_data_node(*d); // d expected to always be valid
                opt = o ? rin::make_data_node(*o) : nullptr;
            }
            QString uri() const { return opt ? opt->descriptor.uri() : dst->descriptor.uri(); }
            reflexive_descriptor& descriptor() { return opt ? opt->descriptor : dst->descriptor; }
            const reflexive_descriptor& descriptor() const { return opt ? opt->descriptor : dst->descriptor; }
        };
        typedef std::unordered_map<data_node_key, job_data> job_map;

        struct work_state
        {
            QElapsedTimer timer;
            usize expected_size;
            usize retrieved;
            int range_start;
            query_operator_flags flags;
        };

        struct validate_result
        {
            std::optional<data_node_index_removal_list> removed = std::nullopt;
            std::optional<data_node_layout_changes> changes = std::nullopt;
            bool validated = false;
        };

        template <detail::is_data_source T>
        bool m_read(data_node& dst, data_node& opt, data_source& source, usize expected_size, query_operator_flags flags);

        template <detail::is_data_source Source>
        bool m_push_result(entity_tree_node& node, reflexive_entity&& e, detail::data_source_container<Source>& src, work_state& state);

        void m_exec(job_frame frame);
        bool m_exec_query(job_data& job, query_operator_flags op);
        void m_remove_from_queue(job_frame frame);
        std::optional<data_node_layout_changes> m_adjust_indexes(entity_tree_node& node);
        validate_result m_validate(entity_tree_node& node);
        std::optional<data_node_layout_changes> m_sort(entity_tree_node& node);
        data_source m_create_source(const reflexive_descriptor& d);
        inline void m_set_state(manager_state state);
        inline void m_interrupt_job_thread();
        inline void m_continue_job_thread(continue_condition condition, std::optional<job_frame> cmd_target = std::nullopt);
        inline void m_evaluate_continue_condition(const job_frame& frame);
        inline void m_clear_interrupt_flag();
        inline reflexive_entity m_create_entity(const QString& parent_id, const QString& id, std::optional<QFileInfo> fsinfo = std::nullopt, bool read_extended = false);
        inline void m_fill_extended_attributes(reflexive_entity& e);

        reflexive_entity m_rename_file_entity(const QString& old_id, const QString& new_id, bool is_indexed);
        reflexive_entity m_copy_file_entity(const QString& src_id, const QString& dst_id, bool is_indexed);
        reflexive_entity m_link_file_entity(const QString& id, const QString& target, bool is_indexed);
        bool m_remove_file_entity(const QString& id, bool is_indexed);
        bool m_recycle_file_entity(const QString& id, bool is_indexed);

        QFileSystemWatcher* m_create_watcher();

        private:
        using enum entity_attribute_type;

        // ******* accessed by multiple threads *******
        mutable QMutex m_mutex;
        QWaitCondition m_continue_exec; // manager waits to be told to continue executing
        
        // read only in main thread, read/write in job thread:
        std::atomic<bool> m_working; // set in m_exec() when job thread is about to start processing a query
        
        // write only in main thread, read/write in job thread:
        std::atomic_flag m_do_interrupt; // callers set this when they want to request an interruption to query processing
        std::vector<QString> m_discards;
        std::unordered_map<query_operator_flags, std::unordered_map<data_node_key, continue_condition>> m_frame_cmds;
        std::optional<continue_condition> m_global_continue_condition;

        // write/cleared in main thread, read/write in job thread:
        std::deque<entity_data_manager::job_frame> m_queue;
        std::unordered_map<query_operator_flags, entity_data_manager::job_map> m_jobs;

        // read/write in main thread, read only in job thread:
        std::unique_ptr<QFileSystemWatcher> m_watcher;
        std::filesystem::path m_database_path;
        std::atomic<int> m_batchsize;
        std::atomic<int> m_deadline; // in milliseconds

        // ******* only accessed by this (job) thread *******
        // affected by continue_condition::Reset and/or Discard_All, and Discard:
        std::unordered_map<QString, data_source> m_sources; // uri is for opt (job_data::opt) if opt is provided
        std::unordered_map<QString, data_node> m_results; // uri always references dst (job_data::dst)
        std::unordered_map<data_node_key, std::unordered_set<QString>> m_invalidated;
        sz::entity_database m_database;
        
        // constants:
        const entity_model* const m_model;
        QFileIconProvider m_icon_provider;
        QMimeDatabase m_mimedb;
    };

    template <detail::is_data_source T>
    inline bool entity_data_manager::m_read(data_node& dst, data_node& opt, data_source& source, usize expected_size, query_operator_flags flags)
    {
        using namespace rin::detail;
        
        data_source_container<T>& src = source;
        work_state state{
            .timer{},
            .expected_size = expected_size,
            .retrieved = 0,
            .range_start = dst->size(),
            .flags = flags
        };
        if (opt)
        {
            for (int i = 0; i < opt->size(); ++i)
            {
                reflexive_entity e = (*opt)[i];
                dst->insert(std::move(e));
            }
        }
        if (m_deadline) {
            state.timer.start();
        }
        while (src.has_data())
        {
            // TODO: when source is the filesystem, figure out how to best send cached QFileInfo objects to create_entity()
            reflexive_entity e = create_entity(opt ? opt : dst, dst->descriptor.text(), src.id_from_source(), flags);
            
            if (m_push_result(*dst, std::move(e), src, state))
            { break; }
        }
        return !src.has_data();
    }

    template <detail::is_data_source Source>
    inline bool entity_data_manager::m_push_result(entity_tree_node& node, reflexive_entity&& e, detail::data_source_container<Source>& src, work_state& state)
    {
        using enum query_status;
        using enum entity_tree_node_state;
        using enum entity_attribute_type;

        auto can_sort = [](const entity_tree_node& n, query_operator_flags flags) -> bool { 
            return (flags & query_operator::Sort) && n.descriptor.sort_info() && (n.size() > 1);
        };        
        const QString name = e.attribute<QString>(Name);
        const int batchsize = m_batchsize.load();
        const bool time_is_up = m_deadline > 0 ? state.timer.hasExpired(static_cast<qint64>(m_deadline)) : false;
        const bool do_interrupt = m_do_interrupt.test();

        m_invalidated[node.key()].erase(name);
        node.insert(std::move(e));
        ++state.retrieved;
        ++src;
        
        bool ret = !src.has_data() || do_interrupt || time_is_up || std::cmp_greater_equal(state.retrieved, batchsize);
        
        if (ret)
        {
            // if the node has a set sort preference, lets sort it now
            std::optional<data_node_layout_changes> index_changes_from_sort = std::nullopt;
            std::optional<data_node_layout_changes> index_changes_from_remove = std::nullopt;
            std::optional<data_node_index_removal_list> index_removals = std::nullopt;

            if (can_sort(node, state.flags) && (node.state() != Updating)) { // sorts do not occur for invalidated nodes in the Updating state
                index_changes_from_sort = m_sort(node);
            }
            if (
                !do_interrupt &&
                (std::cmp_greater_equal(node.descriptor.processed_count(), state.expected_size) || (!src.has_data()))
            ) {
                if (can_sort(node, state.flags) && node.state() == Updating) {
                    index_changes_from_sort = m_sort(node);
                }
                if (validate_result vr = m_validate(node); vr.validated)
                {
                    node.set_state(Valid);
                    node.descriptor.set_status(Completed);
                    index_removals = vr.removed;
                    index_changes_from_remove = vr.changes;
                }
            }
            const int end = node.size() - 1;
            node.descriptor.set_processed_count(node.descriptor.processed_count() + state.retrieved);
            
            if ((state.range_start <= end) || node.descriptor.status() == Completed)
            {
                const QString uri = node.descriptor.uri();
                
                m_results[uri] = rin::make_data_node(node);

                emit data_available(data_node_update{
                    .node = m_results[uri],
                    .start = state.range_start,
                    .end = end,
                    .flags = state.flags,
                    .indexes_changed_from_sort = index_changes_from_sort,
                    .indexes_changed_from_remove = index_changes_from_remove,
                    .indexes_removed = index_removals
                });
            }
            state.range_start = end + 1;

            if ((node.descriptor.status() != Completed) && time_is_up && (!do_interrupt) && std::cmp_less(state.retrieved, batchsize))
            {
                ret = false;
                state.timer.restart();
            }
        }
        return ret;
    }

} // namespace rin
