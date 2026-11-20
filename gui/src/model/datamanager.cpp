// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <algorithm>
#include <QtCore/QEvent>
#include <QtCore/QCoreApplication>
#include <suzuri/utility/string.hpp>
#include <suzuri/utility/thumbnail.hpp>
#include <suzuri/utility/visitor.hpp>
#include <suzuri/metadata/tags.hpp>
#include "entitymodel.hpp"
#include "datamanager.hpp"

namespace rin
{
    entity_data_manager::entity_data_manager(const std::filesystem::path& database_path, entity_model* model, QObject* parent) :
        QThread(parent),
        m_working(false),
        m_watcher(nullptr),
        m_database_path(database_path),
        m_batchsize(10000),
        m_deadline(0),
        m_model(model)
    {
        m_global_continue_condition = std::nullopt;
        m_clear_interrupt_flag();
        start(QThread::Priority::LowPriority);
    }

    entity_data_manager::~entity_data_manager()
    {
        request_abort();
        wait();
    }

    void entity_data_manager::set_watcher_enabled(bool enabled)
    {
        QMutexLocker locker(&m_mutex);

        if (enabled && m_watcher) {
            return;
        }
        m_watcher.reset(enabled ? m_create_watcher() : nullptr);
    }

    // eventually this function will do more than just add a path to the filesystem watcher
    void entity_data_manager::set_watched(const QString& uri, bool watched)
    {
        QMutexLocker locker(&m_mutex);

        if (m_watcher)
        {
            const std::string uri_ = uri.toStdString();
            const auto [id, uri_type] = sz::id_from_uri(uri_);

            if (uri_type == sz::entity_type::File)
            {
                const auto path = QString::fromUtf8(id.data(), static_cast<qsizetype>(id.size()));
                if (watched) {
                    m_watcher->addPath(path);
                }
                else {
                    m_watcher->removePath(path);
                }
            }
        }
    }

    void entity_data_manager::set_batchsize(int size)
    {
        m_batchsize = size;
    }

    void entity_data_manager::set_deadline(int ms)
    {
        m_deadline = ms;
    }

    void entity_data_manager::set_database(const std::filesystem::path& path)
    {
        QMutexLocker locker(&m_mutex);

        m_interrupt_job_thread();
        m_database_path = path;
        m_continue_job_thread(continue_condition::Reset);
    }

    void entity_data_manager::clear()
    {
        QMutexLocker locker(&m_mutex);

        m_interrupt_job_thread();
        m_queue.clear();
        m_jobs.clear();
        
        if (m_watcher) {
            m_watcher->removePaths(m_watcher->directories());
        }
        m_continue_job_thread(continue_condition::Discard_All);
    }

    void entity_data_manager::request_abort()
    {
        QMutexLocker locker(&m_mutex);

        requestInterruption();
        m_interrupt_job_thread();
        m_continue_job_thread(continue_condition::None);
    }

    void entity_data_manager::enqueue(data_node dst, data_node opt, query_operator_flags op, enqueue_position pos)
    {
        using enum enqueue_position;
        using enum continue_condition;
        
        assert(dst);
        
        QMutexLocker locker(&m_mutex);

        m_interrupt_job_thread();

        const QString uri = opt ? opt->descriptor.uri() : dst->descriptor.uri();
        const data_node_key key = dst->key();
        const bool replace = m_jobs[op].contains(key);

        if (replace)
        {
            m_jobs[op].erase(key);
            m_discards.push_back(uri);
        }
        m_jobs[op].emplace(key, job_data{
            .dst = dst,
            .opt = opt
        });
        m_jobs[op][key].set_locked(true);
        
        switch (pos)
        {
        default:
        case Front: { m_queue.emplace_front(key, op, false); break; }
        case Back:  { m_queue.emplace_back(key, op, false); break; }
        }
        m_continue_job_thread(replace ? Discard : None);
    }

    void entity_data_manager::dequeue(data_node_key key, query_operator_flags op)
    {
        QMutexLocker locker(&m_mutex);

        m_interrupt_job_thread();

        auto remove_and_discard = [this, key](const job_data& job, query_operator_flags flags) -> void
        {
            m_discards.emplace_back(job.uri());
            m_remove_from_queue({key, flags});
        };
        if (op && m_jobs[op].contains(key)) {
            remove_and_discard(m_jobs[op][key], op);
        }
        else
        {
            for (auto& [op_flags, map] : m_jobs)
            {
                if (map.contains(key)) {
                    remove_and_discard(map[key], op_flags);
                }
            }
        }
        m_continue_job_thread(continue_condition::Discard);
    }

    data_source entity_data_manager::create_source(const reflexive_descriptor& d)
    {
        return m_create_source(d);
    }

    reflexive_entity entity_data_manager::create_entity(const data_node& data, QString parent_id, QString id, query_operator_flags flags)
    {
        Q_UNUSED(data);
        return m_create_entity(parent_id, id, std::nullopt, flags & query_operator::Read_Extended);
    }

    // iterate over a source, signaling to any connected objects (via push_result()) after
    //   - m_deadline milliseconds (continues iterating)
    //   - when m_batchsize is reached, (stops)
    //   - or when the source has been exhausted (stops)
    // returns true if iteration over the source has completed or m_batchsize items were retrieved (m_batchsize >= query size)
    // returns false if an interruption occured or if m_batchsize items were reached (batch_size < query size)
    // this function assumes there is no active mutex lock and no other thread attempting to read/write internal data structures
    bool entity_data_manager::read(data_node& dst, data_node& opt, data_source& source, query_operator_flags flags)
    {
        using enum query_status;
        using enum query_source;
        using enum entity_tree_node_state;
        using namespace rin::detail;

        usize expected_size = dst->size();
        bool done = true;
        
        if (reflexive_descriptor& d = opt ? opt->descriptor : dst->descriptor; d.status() == Initializing)
        {
            d.set_size(source.size());
            d.set_status(In_Progress);
            expected_size += d.size();
            if (opt) {
                expected_size += opt->size();
            }
        }
        if (const data_node_key key = dst->key(); dst->state() == Invalidated)
        {
            m_invalidated[key].reserve(dst->size());
            m_invalidated[key].insert_range(*dst);
            dst->set_state(Updating);
        }
        dst->reserve(std::min(static_cast<usize>(dst->size()) + m_batchsize, expected_size));

        if (expected_size)
        {
            auto do_read_impl = [&]<typename Source>(data_source& src) -> bool
            {
                return m_read<Source>(dst, opt, src, expected_size, flags);
            };
#define do_read(S, X) do_read_impl.template operator()<S>(X)
            switch (source.type())
            {
            case Filesystem: { done = do_read(filesystem_source, source); break; }
            case Index: { done = do_read(database_source, source); break; }
            case Concept: { done = do_read(concept_source, source); break; }
            default:
            {
                qWarning() << "entity_data_manager: tried to use unknown source";
                break;
            }
            }
#undef do_read
        }
        if (done)
        {
            const QString uri = opt ? opt->descriptor.uri() : dst->descriptor.uri();

            m_sources.erase(uri);
            m_results.erase(uri);
        }
        return done;
    }

    bool entity_data_manager::event(QEvent* e)
    {
        if (e->type() == QEvent::DeferredDelete && isRunning())
        {
            request_abort();
            if (!wait(5000))
            {
                if (QCoreApplication::closingDown()) {
                    terminate();
                }
                else {
                    connect(this, &QThread::finished, this, [this]{ delete this; });
                }
                return true;
            }
        }
        return QThread::event(e);
    }

    void entity_data_manager::run()
    {
        // termination is only enabled in m_exec when we're about to start a job
        // it is diasabled again when a job function returns
        setTerminationEnabled(false);
        forever
        {
            QMutexLocker locker(&m_mutex);
            
            while (!isInterruptionRequested() && m_queue.empty())
            {
                // nothing to do. wait for work to arrive 
                m_continue_exec.wait(&m_mutex);
            }
            if (isInterruptionRequested()) {
                break;
            }
            m_exec(m_queue.front());
        }
    }

    // must always be called with m_mutex locked
    // TODO: system for reusing query results
    // TODO: a node with a non-zero key is assumed to be a standard entity_tree_node,
    //       which only uses query_descriptor for its descriptor. In order to make it
    //       possible to start different types of jobs (like for a node with a concept_descriptor),
    //       we need to figure out the key situation. A possible solution would be to
    //       just use negative ints to specify that the node (dst) is for a random query,
    //       then the model could have a separate keymap to store the results for said queries.
    //       The random query system would help facilitate the implementation of the
    //       query-result-reuse system.
    void entity_data_manager::m_exec(job_frame frame)
    {
        m_clear_interrupt_flag();
        m_evaluate_continue_condition(frame);
        if (m_jobs[frame.op].contains(frame.key))
        {
            job_data job = m_jobs[frame.op][frame.key];
            if (job.dst->locked) // node is locked in enqueue() upon initial insert, unlock here after copying the node
            {
                m_jobs[frame.op][frame.key].copy(job.dst, job.opt);
                job.set_locked(false);      // unlock the model's copy now
                job = m_jobs[frame.op][frame.key]; // reset job to our local copy
            }
            m_set_state(manager_state::Working); // mutex unlock
            frame.done = m_exec_query(job, frame.op); // those who do not work, do not eat
            m_set_state(manager_state::Idle); // mutex lock
            // no need to reset job into job_map here
        }
        else {
            m_remove_from_queue(frame);
        }
        m_evaluate_continue_condition(frame);
        m_clear_interrupt_flag();
    }

    bool entity_data_manager::m_exec_query(job_data& job, query_operator_flags op)
    {
        using namespace query_operator;

        if (op & Read)
        {
            const QString uri = job.uri();

            if (!m_sources.contains(uri)) {
                m_sources.emplace(uri, create_source(job.descriptor()));
            }
            return read(job.dst, job.opt, m_sources.at(uri), op);
        }
        else if ((op & Sort) && job.dst->descriptor.sort_info() && job.dst->size() > 1)
        {
            const QString uri = job.dst->descriptor.uri();
            auto index_changes = m_sort(*job.dst);

            m_results[uri] = rin::make_data_node(*job.dst);

            emit data_available(data_node_update{
                .node = m_results[uri],
                .start = 0,
                .end = job.dst->size() - 1,
                .flags = op,
                .indexes_changed_from_sort = index_changes,
                .indexes_changed_from_remove = std::nullopt,
                .indexes_removed = std::nullopt
            });
        }
        return true;
    }

    void entity_data_manager::m_remove_from_queue(job_frame frame)
    {
        auto new_end = std::remove_if(m_queue.begin(), m_queue.end(),
        [&](const job_frame& f) -> bool
        {
            return frame.key == f.key && frame.op == f.op;
        });
        m_queue.erase(new_end, m_queue.end());
    }

    std::optional<data_node_layout_changes> entity_data_manager::m_adjust_indexes(entity_tree_node& node)
    {
        if (query_descriptor& qd = node.descriptor; qd.keymap)
        {
            std::unordered_map<data_node_key, int>& keymap = *qd.keymap;
            QList<QModelIndex> old_indexes;
            QList<QModelIndex> new_indexes;

            for (int i = 0; i < node.size(); ++i)
            {
                if (const data_node_key k = node[i].key(); keymap.contains(k))
                {
                    const reflexive_entity& e = node[i]; // i is the new index
                    old_indexes << m_model->create_index(e, keymap[k]);
                    new_indexes << m_model->create_index(e, i);
                    keymap[k] = i;
                    // the node index is updated later in the model itself
                }
            }
            if (new_indexes.size()) {
                return std::make_tuple(old_indexes, new_indexes);
            }
        }
        return std::nullopt;
    }

    entity_data_manager::validate_result entity_data_manager::m_validate(entity_tree_node& node)
    {
        using enum entity_tree_node_state;
        
        validate_result result;
        result.validated = true;

        if (const data_node_key key = node.key(); node.state() == Updating && m_invalidated.contains(key) && m_invalidated[key].size())
        {
            data_node_index_removal_list removed;
            for (const QString& name : m_invalidated[key]) // remove all entities from node that are still in the invalidated list
            {
                if (m_do_interrupt.test())
                {
                    result.validated = false;
                    break;
                }
                if (node.contains(name)) {
                    removed.emplace_back(name, node.index_of(name));
                }
            }
            if (result.validated)
            {
                if (node.descriptor.type() == descriptor_type::Query)
                {
                    for (const auto& [name, idx] : removed) {
                        node.erase(name);
                    }
                    result.removed = removed;
                    result.changes = m_adjust_indexes(node);
                }
                m_invalidated.erase(key);
            }
        }
        return result;
    }

    std::optional<data_node_layout_changes> entity_data_manager::m_sort(entity_tree_node& node)
    {
        rin::sort_entities(node);

        if (node.descriptor.type() == descriptor_type::Query) {
            return m_adjust_indexes(node);
        }
        return std::nullopt;
    }

    data_source entity_data_manager::m_create_source(const reflexive_descriptor& d)
    {
        using namespace rin::detail;
        using enum query_source;

        const QString text = d.text();
        const std::string text_ = text.toStdString();
        auto source_type = d.source();
        
        if (source_type == Filesystem)
        {
            if (std::error_code ec; std::filesystem::is_regular_file(text_, ec) && !ec) {
                return data_source(m_database.tags(sz::entity_database_query{ .string = text_ }));
            }
            return data_source(filesystem_source{.dir = QDirListing(text), .it = QDirListing::const_iterator()});
        }
        else if (source_type == Concept) // TODO: concept system
        {
            if (text.startsWith(s_query_cmd_simto.data(), Qt::CaseInsensitive))
            {
                return data_source(m_database.tags(sz::entity_database_query{
                    .string = text_.substr(s_query_cmd_simto.size()),
                    .get_similar = true
                }));
            }
            else if (text.startsWith(s_query_cmd_taglist.data(), Qt::CaseInsensitive))
            {
                if (const std::string s = text_.substr(s_query_cmd_taglist.size()); s.size())
                {
                    return data_source(m_database.tags(sz::entity_database_query{
                        .string = s
                    }));
                }
                return data_source(m_database.tags());
            }
        }
        return data_source(m_database.files(sz::entity_database_query{
            .string = text_
        }));
    }

    inline void entity_data_manager::m_set_state(entity_data_manager::manager_state state)
    {
        using enum entity_data_manager::manager_state;

        switch (state)
        {
        case Idle:
        {
            m_mutex.lock();
            setTerminationEnabled(false);
            m_working = false;
            break;
        }
        case Working:
        {
            m_mutex.unlock();
            setTerminationEnabled(true);
            m_working = true;
            break;
        }
        default: { throw sz::exception(sz::result_code::Invalid_Argument, "unknown state"); }
        }
    }

    // the job thread will reset the variables changed here after the interrupt completes
    inline void entity_data_manager::m_interrupt_job_thread()
    {
        m_do_interrupt.test_and_set();
    }

    // the continue condition is checked after the thread starts again
    // the condition will only be set if it is a higher 'severity' than the one currently set
    // from least to highest severity: None -> Discard -> Discard_All -> Reset
    inline void entity_data_manager::m_continue_job_thread(continue_condition condition, std::optional<job_frame> cmd_target)
    {
        auto weigh_condition = [&](continue_condition c1, continue_condition c2) -> continue_condition
        {
            return continue_condition(std::max(std::to_underlying(c1), std::to_underlying(c2)));
        };
        if (m_queue.size())
        {
            job_frame frame = cmd_target ? *cmd_target : m_queue.front();

            if (m_frame_cmds[frame.op].contains(frame.key))
            {
                auto& c = m_frame_cmds[frame.op][frame.key];
                c = weigh_condition(c, condition);
            }
            else {
                m_frame_cmds[frame.op][frame.key] = condition;
            }
        }
        else
        {
            if (m_global_continue_condition) {
                *m_global_continue_condition = weigh_condition(*m_global_continue_condition, condition);
            }
            else {
                m_global_continue_condition = condition;
            }
        }
        m_continue_exec.notify_all();
    }

    inline void entity_data_manager::m_evaluate_continue_condition(const job_frame& frame)
    {
        using enum continue_condition;

        continue_condition cond = None;
        
        if (m_global_continue_condition)
        {
            cond = *m_global_continue_condition;
            m_global_continue_condition = std::nullopt;
        }
        else
        {
            cond = m_frame_cmds[frame.op][frame.key]; // always expected to have a value
            m_frame_cmds[frame.op][frame.key] = continue_condition::None;
        }
        switch (cond)
        {
        default:
        case None:
        {
            if (frame.done)
            {
                m_jobs[frame.op].erase(frame.key);
                m_remove_from_queue(frame);
                emit job_complete(frame.key, frame.op);
            }
            break;
        }
        // for Reset and Discard_All, it is assummed that the queue and job map are changed in the main thread
        case Reset:
        {
            m_database.clear_cache();
            m_sources.clear();
            m_database.set_database(m_database_path);
            m_frame_cmds.clear();
            [[fallthrough]];
        }
        case Discard_All:
        {
            if (cond != Reset)
            {
                // we have to check if it hasn't already been cleared via Reset to prevent
                // sz::sqlite::database_query's destructor from trying to finalize on a
                // closed database connection
                m_sources.clear();
            }
            m_results.clear();
            m_discards.clear();
            m_invalidated.clear();
            break;
        }
        case Discard:
        {
            // the new job for the given key is already set by the main thread
            // the current node will be destroyed provided its reference count reaches 0
            for (const QString& uri : m_discards)
            {
                m_sources.erase(uri);
                m_results.erase(uri);
            }
            m_discards.clear();
            break;
        }
        }
    }

    inline void entity_data_manager::m_clear_interrupt_flag()
    {
        m_do_interrupt.clear();
    }

    inline reflexive_entity entity_data_manager::m_create_entity(const QString& parent_id, const QString& id, std::optional<QFileInfo> fsinfo, bool read_extended)
    {
        auto e = reflexive_entity::from_uri_or_id(id, nullptr, fsinfo);

        e.set_attribute(Parent_Id, parent_id);
        if (read_extended) {
            m_fill_extended_attributes(e);
        }
        return e;
    }

    inline void entity_data_manager::m_fill_extended_attributes(reflexive_entity& e)
    {
        e.fill_extended_attributes(m_database, m_mimedb, false);
    }

    // for filesystem-based queries, the parent dir of new_id is the same as qd.query_string
    // TODO: if update_entity fails, should we undo the rename?
    reflexive_entity entity_data_manager::m_rename_file_entity(const QString& old_id, const QString& new_id, bool is_indexed)
    {
        reflexive_entity e;

        if (QFile::rename(old_id, new_id))
        {
            if (is_indexed)
            {
                const sz::indexed_result_code result = m_database.update_entity(
                    old_id.toStdString(),
                    std::vector<std::string>{std::format("id={}", new_id.toStdString())},
                    false
                );
                assert(result.code == sz::result_code::Success);
            }
            const QFileInfo info(new_id);

            e = m_create_entity(info.absoluteDir().path(), new_id, info);
        }
        return e;
    }

    reflexive_entity entity_data_manager::m_copy_file_entity(const QString& src_id, const QString& dst_id, bool is_indexed)
    {
        reflexive_entity e;

        if (QFile::copy(src_id, dst_id))
        {
            if (is_indexed)
            {
                sz::entity_database_info entity_info = m_database.entity_info(src_id.toStdString());

                entity_info.id = dst_id.toStdString();
                
                const sz::result_code rc = m_database.register_entity(entity_info);
                
                assert(rc == sz::result_code::Success);
            }
            const QFileInfo info(dst_id);

            e = m_create_entity(info.absoluteDir().path(), dst_id, info);
        }
        return e;
    }

    reflexive_entity entity_data_manager::m_link_file_entity(const QString& id, const QString& target, bool is_indexed)
    {
        reflexive_entity e;

        if (QFile::link(id, target))
        {
            if (is_indexed)
            {
                // TODO: how to handle this?
            }
            const QFileInfo info(target);
            
            // should entity type be File for symlinks?
            e = m_create_entity(info.absoluteDir().path(), target, info);
        }
        return e;
    }

    bool entity_data_manager::m_remove_file_entity(const QString& id, bool is_indexed)
    {
        const QFileInfo info(id);
        const bool removed = (info.isFile() || info.isSymLink()) ? QFile::remove(id) : QDir(id).removeRecursively();

        // TODO: delete_entity should return a bool indicating success
        if (is_indexed) {
            m_database.delete_entity(id.toStdString());
        }
        return removed;
    }

    bool entity_data_manager::m_recycle_file_entity(const QString& id, bool is_indexed)
    {
        // TODO: entity_database needs to support the recycle case
        Q_UNUSED(is_indexed);
        // QString trash_location;
        // const bool success = QFile::moveToTrash(id, &trash_location);
        // qDebug() << "item: " << id << " moved to trash: " << trash_location;
        // return success;
        return QFile::moveToTrash(id);
    }

    QFileSystemWatcher* entity_data_manager::m_create_watcher()
    {
        auto* watcher = new QFileSystemWatcher(this);

        auto notify_for_update = [this](const QString& path) -> void
        {
            qDebug() << "entity_data_manager: notify for update: " << path;
            const QFileInfo info(path);
            const QString dir = info.isDir() ? path : info.canonicalPath();
            emit data_changed(QString::fromStdString(sz::entity_uri(dir.toStdString(), sz::entity_type::File)));
        };
        connect(watcher, &QFileSystemWatcher::directoryChanged, this, notify_for_update);
        connect(watcher, &QFileSystemWatcher::fileChanged, this, notify_for_update);
        return watcher;
    }

} // namespace rin
