// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <algorithm>
#include <QtCore/QEvent>
#include <QtCore/QCoreApplication>
#include <suzuri/utility/string.hpp>
#include <suzuri/metadata/tags.hpp>
#include "datamanager.hpp"

namespace rin
{
    entity_data_manager::entity_data_manager(const std::filesystem::path& database_path, QObject* parent)
    : QThread(parent), m_mutex(), 
    m_database(database_path), m_watcher(nullptr),
    m_batchsize(10000), m_deadline(200), m_watcher_enabled(false)
    {
        start(QThread::Priority::LowPriority);
        m_do_interrupt.clear();
    }

    entity_data_manager::~entity_data_manager()
    {
        m_request_abort();
        
        if (m_watcher)
        { delete m_watcher; }
        
        wait();
    }

     void entity_data_manager::watch_path(const QString& path)
    {
        QMutexLocker locker(&m_mutex);
        m_interrupt_and_wait_for_job_thread();
        if (m_watcher_enabled)
        {
            qDebug() << "entity_data_manager: watch path: " << path;
            m_watcher->addPath(path);
        }
    }

    void entity_data_manager::unwatch_path(const QString& path)
    {
        QMutexLocker locker(&m_mutex);
        m_interrupt_and_wait_for_job_thread();
        if (m_watcher_enabled && m_watcher->directories().contains(path))
        {
            qDebug() << "entity_data_manager: unwatch path: " << path;
            m_watcher->removePath(path);
        }
    }

    void entity_data_manager::set_watcher_enabled(bool enabled)
    {
        QMutexLocker locker(&m_mutex);
        m_interrupt_and_wait_for_job_thread();

        if (m_watcher_enabled == enabled)
        { return; }

        m_watcher_enabled = enabled;
        if (!m_watcher_enabled)
        {
            delete m_watcher;
            m_watcher = nullptr;
        }
        else
        { m_watcher = m_create_watcher(); }
    }

    void entity_data_manager::set_batchsize(size_t size)
    {
        QMutexLocker locker(&m_mutex);
        m_interrupt_and_wait_for_job_thread();
        m_batchsize = size;
    }

    void entity_data_manager::set_deadline(int ms)
    {
        QMutexLocker locker(&m_mutex);
        m_interrupt_and_wait_for_job_thread();
        m_deadline = ms;
    }

    void entity_data_manager::set_database(const std::filesystem::path& path)
    {
        QMutexLocker locker(&m_mutex);
        m_interrupt_and_wait_for_job_thread();
        m_database.set_database(path);
    }

    void entity_data_manager::clear()
    {
        QMutexLocker locker(&m_mutex);
        m_interrupt_and_wait_for_job_thread();
        m_queries.clear();
        m_fetches.clear();
        m_sources.clear();
        m_invalidated.clear();
        m_nodes.clear();
        m_extended_reads.clear();
        m_extended_read_positions.clear();
        m_pending_sort.clear();
        m_pending_sort_keys.clear();
        m_database.clear_cache();
        if (m_watcher_enabled)
        {
            m_watcher->removePaths(m_watcher->directories());
        }
    }

    void entity_data_manager::request_abort()
    {
        m_request_abort();
    }

    // possible queries and their syntax:
    //  (1) absolute directory path (i.e. /home/user/Pictures): produce all files and dirs in that directory
    //  (2) tag query (i.e. cat,dog|bird): produce all files/dirs/entities/etc. that are tagged with 
    //  the tags given in the query string (see <suzuri/entity/database.hpp> for the query syntax)
    //  (3) "tagsof:{query}" produce all of the tags that occur in the set produced by query, along with the count of times each tag appears
    int entity_data_manager::query(const QString& short_text, entity_tree& tree, int index, int parent, std::optional<entity_attribute_type> sort_key, std::optional<Qt::SortOrder> sort_order)
    {
        qDebug() << "entity_data_manager: call query(): " << short_text;

        QMutexLocker locker(&m_mutex);
        m_interrupt_and_wait_for_job_thread();

        auto query_type = m_query_type(short_text);
        const auto ft = query_descriptor::full_text(short_text, query_type);

        int k = 0;
        if (m_nodes.contains(ft))
        {
            k = m_nodes[ft].key;
        }
        else
        {
            assert(!m_sources.contains(ft));
            m_sources.emplace(ft, m_create_source(short_text, query_type));
            query_descriptor qd = m_create_query_descriptor(short_text, m_sources.at(ft));
            if (sort_key && sort_order)
            {
                qd.sort_key = sort_key;
                qd.sort_order = sort_order;
            }
            k = tree.make_node(qd, index, parent);
            m_nodes.emplace(ft, std::as_const(tree[k]));
            m_queries.push_front({ft, query_descriptor::query_mode::Read});
        }
        m_tell_job_thread_to_continue();
        m_have_query.wakeAll();
        return k;
    }

    int entity_data_manager::query(const QString& short_text, entity_tree& tree, const QList<QUrl>& urls, int index, int parent)
    {
        using enum query_descriptor::query_type;
        qDebug() << "entity_data_manager: call query() for concept: " << short_text;

        QMutexLocker locker(&m_mutex);
        m_interrupt_and_wait_for_job_thread();

        const QString ft = short_text.startsWith("concept://") ? short_text : query_descriptor::full_text(short_text, Concept);        
        int k = 0;
        if (m_nodes.contains(ft))
        {
            k = m_nodes[ft].key;
        }
        else
        {
            m_sources.emplace(ft, m_create_source(urls));
            const auto qd = m_create_query_descriptor(short_text, m_sources.at(ft));
            k = tree.make_node(qd, index, parent);
            m_nodes.emplace(ft, std::as_const(tree[k]));
            m_queries.push_front({ft, qd.mode});
        }
        m_tell_job_thread_to_continue();
        m_have_query.wakeAll();
        return k;
    }

    void entity_data_manager::query(const entity_tree_node& node)
    {
        const QString ft = node.descriptor.full_text();
        qDebug() << "entity_data_manager: call query() for extended read of: " << ft;

        QMutexLocker locker(&m_mutex);
        m_interrupt_and_wait_for_job_thread();

        if (!m_extended_reads.contains(ft))
        {
            assert(!m_extended_read_positions.contains(ft));
            m_queries.push_front({ft, query_descriptor::query_mode::Read_Extended});
            m_extended_reads.emplace(ft, node);
            m_extended_read_positions.emplace(ft, 0);
        }
        m_tell_job_thread_to_continue();
        m_have_query.wakeAll();
    }

    void entity_data_manager::sort(entity_tree_node& node, const std::unordered_map<int, int>& keys)
    {
        const QString ft = node.descriptor.full_text();
        qDebug() << "entity_data_manager: call sort(): " << ft;

        QMutexLocker locker(&m_mutex);
        m_interrupt_and_wait_for_job_thread();

        using enum query_descriptor::query_mode;
        if (!m_pending_sort.contains(ft))
        {
            assert(!m_pending_sort_keys.contains(ft));
            node.descriptor.mode = Sort;
            m_queries.push_front({ft, Sort});
            m_pending_sort.emplace(ft, std::as_const(node));
            m_pending_sort_keys.emplace(ft, keys);
        }
        m_tell_job_thread_to_continue();
        m_have_query.wakeAll();
    }

    void entity_data_manager::fetch(const QString& query_string, const std::vector<entity_model_url>& urls, fetch_action action, bool is_update)
    {
        qDebug() << "entity_data_manager: call fetch(): " << query_string << ", url count: " << urls.size();

        QMutexLocker locker(&m_mutex);
        m_interrupt_and_wait_for_job_thread();
        if (!m_fetches.contains(query_string))
        {
            m_create_fetch_query(query_string, urls, action, is_update);
            m_queries.push_front({query_string, query_descriptor::query_mode::Fetch});
        }
        m_tell_job_thread_to_continue();
        m_have_query.wakeAll();
    }

    // synchronous version
    int entity_data_manager::tag(const std::string& entity_id, const std::vector<std::string>& tags)
    {
        QMutexLocker locker(&m_mutex);
        m_interrupt_and_wait_for_job_thread();
        
        sz::metadata::parameters args{
            .tags = tags,
            .sidecar_keys{},
            .sidecar_split_character{},
            .sidecar_alternate_root_path{},
            .sidecar_type{},
            .use_sidecars = false,
            .check_tags = false
        };

        std::filesystem::path path = entity_id;
        int tags_applied = 0;
        sz::metadata::tag_input(path, m_database, args,
            [&](std::filesystem::path p, const std::vector<std::string>& tags, sz::result_code rc) -> bool
            {
                qDebug() << "tag item: " << p.string() << " with: " << tags;
                ++tags_applied;
                return false;
            },
            [](std::string, std::string, sz::result_code) -> bool
            {
                return false;
            }, false, false
        );
        m_tell_job_thread_to_continue();
        return tags_applied;
    }

    index_descriptor entity_data_manager::tag(const QString& entity_id, const sz::metadata::parameters& args)
    {
        using enum query_descriptor::query_mode;
        QMutexLocker locker(&m_mutex);
        m_interrupt_and_wait_for_job_thread();

        if (!m_indexed_writes.contains(entity_id))
        {
            auto qd = m_create_query_descriptor(entity_id, args);
            qd.status = query_descriptor::query_status::Initializing;
            m_queries.emplace_front(entity_id, qd.mode);
            m_indexed_writes.emplace(entity_id, index_descriptor{
                .query = qd,
                .args = args
            });
        }
        const auto qd = m_indexed_writes.at(entity_id);
        m_tell_job_thread_to_continue();
        m_have_query.wakeAll();
        return qd;
    }

    void entity_data_manager::continue_query()
    {
        qDebug() << "entity_data_manager: call continue_query()";

        QMutexLocker locker(&m_mutex);
        m_tell_job_thread_to_continue();
    }

    void entity_data_manager::continue_query(const query_descriptor& qd)
    {
        using enum query_descriptor::query_mode;
        QMutexLocker locker(&m_mutex);
        m_interrupt_and_wait_for_job_thread();

        const QString ft = qd.full_text();
        if (!m_is_in_queue(qd))
        {
            // use query() to start a new query
            qWarning() << "entity_data_manager: invalid use of continue_query(qd). qd was: " << ft;
            return;
        }
        
        qDebug() << "entity_data_manager: continue query for: " << ft;

        if (const auto& [s, m] = m_queries.front(); s == ft && m == qd.mode)
        { m_tell_job_thread_to_continue(); }
        else
        {
            m_remove_from_queue(qd);
            m_queries.push_front({ft, qd.mode});
            m_tell_job_thread_to_continue();
        }
    }

    void entity_data_manager::cancel_query(const query_descriptor& qd)
    {
        using enum query_descriptor::query_mode;
        QMutexLocker locker(&m_mutex);
        m_interrupt_and_wait_for_job_thread();

        const QString ft = qd.full_text();
        qDebug() << "entity_data_manager: call cancel_query() for: " << ft << ", mode: " << std::to_underlying(qd.mode);
        
        m_erase_job_data(ft, qd.mode);
        m_remove_from_queue(ft, qd.mode);
    }

    void entity_data_manager::cancel_query(const QString& text, query_descriptor::query_mode mode)
    {
        using enum query_descriptor::query_mode;
        QMutexLocker locker(&m_mutex);
        m_interrupt_and_wait_for_job_thread();

        qDebug() << "entity_data_manager: call cancel_query() for: " << text << ", mode: " << std::to_underlying(mode);

        m_erase_job_data(text, mode);
        m_remove_from_queue(text, mode);
    }

    bool entity_data_manager::event(QEvent* e)
    {
        if (e->type() == QEvent::DeferredDelete && isRunning())
        {
            m_request_abort();
            if (!wait(5000))
            {
                if (QCoreApplication::closingDown())
                { terminate(); }
                else
                { connect(this, &QThread::finished, this, [this]{ delete this; }); }
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
            
            while (!isInterruptionRequested() && m_queries.empty())
            {
                // nothing to do. wait for work to arrive 
                m_have_query.wait(&m_mutex);
            }

            if (isInterruptionRequested())
            { return; }
            
            if (m_exec(m_mutex, std::as_const(m_queries).front()))
            { return; }
        }
    }

    void entity_data_manager::m_request_abort()
    {
        requestInterruption(); // thread safe
        QMutexLocker locker(&m_mutex);
        
        // wake ourself up to prepare for abort
        // we don't want to wait here for any reason, so we dont call the usual m_interrupt_and_wait_for_job_thread()
        m_do_interrupt.test_and_set();
        m_have_query.wakeAll();
        m_tell_job_thread_to_continue();
    }

    // expects caller to be holding m_mutex in a locked state
    void entity_data_manager::m_interrupt_and_wait_for_job_thread()
    {
        m_do_interrupt.test_and_set();
        if (m_working)
        { m_no_work_being_performed.wait(&m_mutex); }
    }

    inline void entity_data_manager::m_tell_job_thread_to_continue()
    {
        m_do_interrupt.clear();
        m_should_continue.test_and_set();
        m_continue_query.wakeAll();
    }

    inline bool entity_data_manager::m_is_in_queue(const query_descriptor& qd)
    {
        const QString ft = qd.full_text();
        return std::any_of(m_queries.begin(), m_queries.end(), [&](const std::tuple<QString, query_descriptor::query_mode>& t) -> bool
        {
            const auto& [s, m] = t;
            return s == ft && m == qd.mode;
        });
    }

    inline void entity_data_manager::m_erase_job_data(const QString& text, query_descriptor::query_mode mode)
    {
        using enum query_descriptor::query_mode;
        switch (mode)
        {
        case Read:
        {
            m_nodes.erase(text);
            m_sources.erase(text);
            m_invalidated.erase(text);
            break;
        }
        case Read_Extended:
        {
            m_extended_reads.erase(text);
            m_extended_read_positions.erase(text);
            break;
        }
        case Sort:
        {
            m_pending_sort.erase(text);
            m_pending_sort_keys.erase(text);
            break;
        }
        case Fetch:
        {
            m_fetches.erase(text);
            break;
        }
        default:
            break;
        }
    }

    inline void entity_data_manager::m_remove_from_queue(const query_descriptor& qd)
    {
        m_remove_from_queue(qd.full_text(), qd.mode);
    }

    inline void entity_data_manager::m_remove_from_queue(const QString& text, query_descriptor::query_mode mode)
    {
        auto new_end = std::remove_if(m_queries.begin(), m_queries.end(),
        [&](const std::tuple<QString, query_descriptor::query_mode>& t) -> bool
        {
            const auto& [s, m] = t;
            return s == text && m == mode;
        });
        m_queries.erase(new_end, m_queries.end());
    }

    int64_t entity_data_manager::m_query_size(const QString& text)
    {
        using enum query_descriptor::query_type;
        using namespace rin::detail;
        int64_t size = 0;
        if (QFile::exists(text))
        {
            QDirListing dir(text);
            for (auto it = dir.begin(); it != dir.end(); ++it)
            { ++size; }
        }
        else // database
        {
            // TODO
            size = static_cast<int64_t>(m_database.files(sz::entity_database_query{.string = text.toStdString()}).size());
        }
        return size;
    }

    bool entity_data_manager::m_query_size(query_descriptor& qd, entity_data_manager::data_source& source, bool emit_signal)
    {
        using enum query_descriptor::query_type;
        using enum query_descriptor::query_status;
        using namespace rin::detail;
        int64_t size = qd.size > 0 ? qd.size : 0;
        bool stopped_early = false;
        auto type = source.type();
        if (type == Filesystem)
        {
            auto& fs_src = std::get<filesystem_source>(source);
            while (fs_src.it != fs_src.dir.end())
            {
                ++size;
                ++fs_src.it;
                if (m_do_interrupt.test())
                {
                    stopped_early = true;
                    break;
                }
            }
            
            if (!stopped_early)
            { fs_src.it = fs_src.dir.begin(); }
            else
            { qd.status = Initializing; }
        }
        else if (type == Indexed)
        {
            const auto& db_src = std::get<database_source>(source);
            size = db_src.size();
        }
        else if (type == Concept)
        {
            const auto& data = std::get<concept_source>(source);
            size = static_cast<int64_t>(data.urls.size());
        }
        qd.size = size;
        
        if (emit_signal)
        { emit data_available(qd, {}); }
        
        return stopped_early;
    }

    query_descriptor::query_type entity_data_manager::m_query_type(const QString& short_text) const
    {
        using enum query_descriptor::query_type;
        if (QFileInfo info(short_text); info.isDir())
        { return Filesystem; }
        else if (info.isFile())
        { return Indexed; }

        if (short_text.endsWith(".concept") || short_text.startsWith("concept://"))
        { return Concept; }

        return Indexed;
    }

    entity_data_manager::data_source entity_data_manager::m_create_source(const QString& short_text, query_descriptor::query_type source_type)
    {
        using namespace rin::detail;
        using enum query_descriptor::query_type;
        if (source_type == Filesystem) // filesystem
        {
            entity_data_manager::data_source s(filesystem_source{.dir = QDirListing(short_text), .it = QDirListing::const_iterator()});
            auto& dirsource = std::get<filesystem_source>(s);
            dirsource.it = dirsource.dir.begin();
            return s;
        }
        
        if (const QFileInfo info(short_text); info.isFile())
        { return entity_data_manager::data_source(m_database.tags(sz::entity_database_query{.string = short_text.toStdString()})); }

        if (short_text.startsWith(s_query_cmd_tagsof.data(), Qt::CaseInsensitive)) // indexed
        {
            // TODO: entity_database needs to forbid starting a tag namespace with "!". it is reserved for the entity_data_manager
            return entity_data_manager::data_source(m_database.tags(sz::entity_database_query{
                .string = short_text.toStdString().substr(s_query_cmd_tagsof.size())
            }));
        }
        
        return entity_data_manager::data_source(m_database.files(sz::entity_database_query{
            .string = short_text.toStdString()
        }));
    }

    entity_data_manager::data_source entity_data_manager::m_create_source(const QList<QUrl>& urls)
    {
        return entity_data_manager::data_source(detail::concept_source{
            .urls = urls,
            .position = 0
        });
    }

    query_descriptor entity_data_manager::m_create_query_descriptor(const QString& short_text, const entity_data_manager::data_source& source) const
    {
        return {
            .text = short_text,
            .size = -1,
            .processed = 0,
            .filesize{},
            .dir_count{},
            .file_count{},
            .tag_count{},
            .type = source.type(),
            .mode = query_descriptor::query_mode::Read,
            .status = query_descriptor::query_status::Initializing
        };
    }

    query_descriptor entity_data_manager::m_create_query_descriptor(const QString& entity_id, const sz::metadata::parameters& args) const
    {
        Q_UNUSED(args); // TODO: derive query size, file_count using args
        return {
            .text = entity_id,
            .type = query_descriptor::query_type::Indexed,
            .mode = query_descriptor::query_mode::Write,
        };
    }

    // TODO: http
    // for now, we assume text refers to a place on the local filesystem
    query_descriptor entity_data_manager::m_create_fetch_query(const QString& text, const std::vector<entity_model_url>& urls, fetch_action action, bool update_only)
    {
        m_fetches.emplace(text, fetch_descriptor{
            .query = query_descriptor{
                .text = text,
                .size = static_cast<int64_t>(urls.size()),
                .processed = 0,
                .filesize{},
                .dir_count{},
                .file_count{},
                .tag_count{},
                .type = query_descriptor::query_type::Filesystem, // TODO
                .mode = query_descriptor::query_mode::Fetch,
                .status = query_descriptor::query_status::Initializing
            },
            .urls = urls,
            .action = action,
            .is_update = update_only
        });
        return m_fetches[text].query;
    }

    
    inline reflexive_entity entity_data_manager::m_make_item(const QString& parent_id, const QString& id, const QFileInfo& fsinfo, sz::entity_type type, bool is_indexed, bool fill_extended_attributes)
    {
        using enum entity_attribute_type;
        const bool is_file = type == sz::entity_type::File;

        // TODO: standardized ids for files
        // TODO: tag entities have children if additional tags occur within their inset
        reflexive_entity item(is_file ? fsinfo.fileName() : id, type, is_indexed);
        item.set_attribute(Parent_Id, parent_id);
        if (is_file)
        {
            item.set_has_children(fsinfo.isDir());
            item.set_attribute(File_Info, fsinfo);
        }
        if (thumbnail_manager::has_thumbnail(id))
        {
            // TODO: thumbnail_manager should return the size for us
            item.set_attribute(Sizehint, QImageReader(id).size());
        }
        if (fill_extended_attributes)
        {
            m_fill_extended_item_attributes(item, id, is_file, is_indexed);
        }
        return item;
    }

    inline void entity_data_manager::m_fill_extended_item_attributes(reflexive_entity& e, const QString& id, bool is_file, bool is_indexed)
    {
        using enum entity_attribute_type;
        if (is_file)
        {
            const auto fsinfo = e.attribute<QFileInfo>();
            const auto mime = m_mimedb.mimeTypeForFile(fsinfo, QMimeDatabase::MatchExtension);
            e.set_attribute(Mime_Type, mime);
            e.set_attribute(Modified, fsinfo.lastModified().toString());
            e.set_attribute(Created, fsinfo.birthTime().toString());
            e.set_attribute(Accessed, fsinfo.lastRead().toString());
            e.set_attribute(File_Type, mime.comment().isEmpty() ? mime.name() : mime.comment());

            if (fsinfo.isFile())
            { e.set_attribute(Size, QLocale::system().formattedDataSize(fsinfo.size())); }
            else
            {
                const int64_t size = m_query_size(id);
                e.set_attribute(Size, QLocale::system().toString(size) + " " + tr("items"));
            }
        }
        if (is_indexed)
        {
            const std::string id_ = id.toStdString();
            const auto dbinfo = m_database.entity_info(id_);
            e.set_attribute(Description, QString::fromStdString(dbinfo.description));
            e.set_attribute(Color, dbinfo.rgba);

            sz::sqlite::database_query tags = m_database.tags(sz::entity_database_query{.string = id_});
            tag_set tagset;
            for (std::string_view tag : tags)
            {
                tagset.emplace(QString::fromUtf8(tag.data(), static_cast<qsizetype>(tag.size())));
            }
            e.set_attribute(Tags, std::move(tagset));
        }
    }

    // returns true if iteration over the source has completed or m_batchsize items were retrieved (m_batchsize >= query size)
    // returns false if an interruption occured or if m_batchsize items were reached (batch_size < query size)
    // this function assumes there is no active mutex lock and no other thread attempting to read/write internal data structures
    bool entity_data_manager::m_run_read_query(entity_tree_node& node)
    {
        // iterate over source, signaling to any connected objects after
        //   - m_deadline milliseconds (continues iterating)
        //   - when m_batchsize is reached, (stops)
        //   - or when the source has been exhausted (stops)
        using enum query_descriptor::query_type;
        using enum query_descriptor::query_status;
        using enum entity_tree_node_state;
        using namespace rin::detail;
        
        const QString full_text = node.descriptor.full_text();
        auto& source = m_sources.at(full_text);

        if (node.descriptor.size <= -1 || node.descriptor.status == Initializing) // TODO: option to start the query anyways even if we don't know the full size
        {
            // uninitialized query. get the size
            if (m_query_size(node.descriptor, source, node.state != Invalidated))  // will emit data_available for us and set status of qd accordingly
            { return false; }
            else
            { node.descriptor.status = In_Progress; }
        }
        if (node.state == Invalidated)
        {
            m_invalidated[full_text].reserve(node.items.size());
            m_invalidated[full_text].insert_range(node);
            node.state = Updating;
        }
        node.reserve(std::min(m_batchsize, static_cast<size_t>(node.descriptor.size)));
        
        switch (source.type())
        {
        case Filesystem: { m_iterate_source(std::get<filesystem_source>(source), node); break; }
        case Indexed: { m_iterate_source(std::get<database_source>(source), node); break; }
        case Concept: { m_iterate_source(std::get<concept_source>(source), node); break; }
        default:
        {
            qDebug() << "entity_data_manager: unknown source type for node: " << node.descriptor.full_text();
            return false;
        }
        }
        
        if (m_do_interrupt.test())
        { return false; }
        
        bool done = node.descriptor.processed >= node.descriptor.size || (source.at_end());
        node.descriptor.status = done ? Completed : In_Progress;
        
        if (done)
        {
            if (node.state == Updating && m_invalidated.contains(full_text) && m_invalidated[full_text].size())
            {
                std::vector<std::tuple<QString, int>> removed;
                for (const QString& name : m_invalidated[full_text])
                {
                    if (m_do_interrupt.test())
                    {
                        done = false;
                        break;
                    }
                    if (const int i = node.index_of(name); i > -1 && i < node.size())
                    {
                        removed.emplace_back(name, i);
                        node.erase(name);
                    }
                    // no need to adjust counts here because invalidated nodes have their descriptors redone from scratch
                }
                if (done)
                {
                    m_invalidated[full_text].clear();
                    emit data_removed(node.descriptor, removed);
                }
            }
        }
        return done;
    }

    bool entity_data_manager::m_run_extended_read_query(entity_tree_node& node, int& position)
    {
        qDebug() << "entity_data_manager: run extended read query for: " << node.descriptor.full_text() << ", start: " << position;
        using enum query_descriptor::query_type;
        using enum entity_tree_node_state;
        using enum entity_attribute_type;
        node.descriptor.mode = query_descriptor::query_mode::Read_Extended;
        node.descriptor.status = query_descriptor::query_status::In_Progress;

        QElapsedTimer timer;
        timer.start();
        int start = position;
        int items_processed = 0;
        for ( ; position < node.size(); ++position)
        {
            if (m_do_interrupt.test())
            { break; }

            reflexive_entity& e = node[position];

            const QString id = e.type() == sz::entity_type::File ? e.attribute<QFileInfo>().absoluteFilePath() : e.attribute<QString>(Name);
            const bool is_file = e.type() == sz::entity_type::File;
            const bool is_indexed = m_database.entity_exists(id.toStdString());

            m_fill_extended_item_attributes(e, id, is_file, is_indexed);
            ++items_processed;
            ++node.descriptor.processed_extended;

            if (timer.hasExpired(m_deadline))
            {
                if (items_processed)
                {
                    emit update_available(node, start, position);
                    start = position + 1;
                    items_processed = 0;
                }
                timer.restart();
            }
        }

        const bool done = position >= node.size();
        if (done)
        {
            node.descriptor.status = query_descriptor::query_status::Completed;
            node.descriptor.mode = query_descriptor::query_mode::Read;
        }
        if (items_processed)
        {
            emit update_available(node, start, start + items_processed - 1);
        }
        return done;
    }

    bool entity_data_manager::m_run_sort_query(entity_tree_node& node, const std::unordered_map<int, int>& keys)
    {
        qDebug() << "entity_data_manager: do async sort for: " << node.descriptor.full_text() << ", size: " << node.descriptor.size;
     
        rin::sort_entities(node);
        std::vector<node_index_change> changes;
        for (int i = 0; std::cmp_less(i, node.size()); ++i)
        {
            if (const int k = node[i].key(); keys.contains(k))
            {
                changes.emplace_back(node_index_change{
                    .key = k,
                    .original_index = keys.at(k),
                    .new_index = i
                });
            }
        }
        emit data_sorted(node, changes);
        return true;
    }

    // TODO: support for http gets. for now we assume urls refers to items on the local filesystem
    // TODO: make a version of m_push_result that doesn't require a data_source
    // TODO: support for directories
    bool entity_data_manager::m_run_fetch_query(fetch_descriptor& fd)
    {
        using enum query_descriptor::query_status;
        using enum fetch_action;
        static const std::set<fetch_action> returning_actions{Copy, Move, Link};

        fd.query.status = In_Progress;
        const QString parent = QDir::toNativeSeparators(fd.query.text) + QDir::separator();
        std::vector<entity_model_url> urls_remaining = fd.urls;
        std::vector<entity_model_url> urls_failed;
        std::vector<entity_model_url> urls_processed;
        std::vector<reflexive_entity> items;
        QElapsedTimer t;
        bool ok = true;

        t.start();
        for (const auto& url : fd.urls)
        {
            if (isInterruptionRequested())
            {
                ok = false;
                break;
            }
            const QString src = url.src.toLocalFile();
            const QString dst = parent + url.target_name;
            const bool is_indexed = m_database.entity_exists(src.toStdString());
            reflexive_entity item;
            bool action_success = false;
            bool return_item = false;

            switch (fd.action)
            {
            case Link:      { item =           m_link_file_entity(src, dst, is_indexed); break; }
            case Copy:      { item =           m_copy_file_entity(src, dst, is_indexed); break; }
            case Move:      { item =           m_rename_file_entity(src, dst, is_indexed); break; }
            case Remove:    { action_success = m_remove_file_entity(src, is_indexed); break; }
            case Recycle:   { action_success = m_recycle_file_entity(src, is_indexed); break; }
            default:
            {
                qWarning() << "entity_data_manager: unknown fetch action type " << std::to_underlying(fd.action);
                action_success = false;
            }
            }

            if (returning_actions.contains(fd.action))
            {
                action_success = item.valid();
                return_item = true;
            }
            
            ok = action_success && ok;
            if (action_success)
            {
                if (return_item)
                {
                    m_adjust_count(fd.query, item, true);
                    items.emplace_back(std::move(item));
                }
                ++fd.query.processed;

                // we expect url to occur exactly once in urls
                auto new_end = std::remove(urls_remaining.begin(), urls_remaining.end(), url);
                urls_remaining.erase(new_end, urls_remaining.end());
                urls_processed.push_back(url);
            }
            else
            { urls_failed.push_back(url); }

            if (t.hasExpired(static_cast<qint64>(m_deadline)))
            {
                if (items.size())
                {
                    if (fd.is_update)
                    { emit update_available(fd.query, items); }
                    else
                    { emit data_available(fd.query, items); }
                    items.clear();
                }
                if (urls_processed.size())
                {
                    emit fetch_progressed(fd, urls_processed);
                    urls_processed.clear();
                }
                t.restart();
            }
        } // for (...)

        if (ok)
        {
            fd.urls.clear();
            fd.query.status = Completed;
        }
        else
        { fd.urls = urls_remaining; }

        if (items.size())
        {
            if (fd.is_update)
            { emit update_available(fd.query, items); }
            else
            { emit data_available(fd.query, items); }
        }
        if (urls_processed.size())
        {
            emit fetch_progressed(fd, urls_processed);
        }

        if (urls_failed.size())
        { emit fetch_failed(fd, urls_failed); }
        
        return ok;
    }

    bool rin::entity_data_manager::m_run_indexed_write_query(index_descriptor& ixd)
    {
        const QString& entity_id = ixd.query.text;
        const std::filesystem::path path = entity_id.toStdString();
        
        QElapsedTimer t;
        QList<std::tuple<QString, int>> processed;
        QList<std::tuple<QString, QString>> failed;
        processed.reserve(128); // TODO: get total amount of files/dirs to be tagged
        ixd.query.status = query_descriptor::query_status::In_Progress;
        
        sz::metadata::tag_input(path, m_database, ixd.args,
            [&](std::filesystem::path p, const std::vector<std::string>& tags, sz::result_code c) -> bool
            {
                Q_UNUSED(c);
                qDebug() << "[entity_data_manager] tag item: " << p.string() << " with: " << tags;

                ++ixd.query.size;
                ++ixd.query.processed;
                
                if (std::filesystem::is_directory(p))
                { ++ixd.query.dir_count; }
                else
                { ++ixd.query.file_count; }

                processed.emplace_back(QString::fromStdString(p.string()), static_cast<int>(tags.size()));

                if (t.hasExpired(static_cast<qint64>(m_deadline)))
                {
                    emit query_progressed(ixd.query, processed);
                    processed.clear();
                }

                return false;
            },
            [&](std::string eid, std::string msg, sz::result_code c) -> bool
            {
                Q_UNUSED(c);
                qWarning() << "[entity_data_manager] failed to apply tags: " << msg;
                failed.emplace_back(QString::fromStdString(eid), QString::fromStdString(msg));

                if (t.hasExpired(static_cast<qint64>(m_deadline)))
                {
                    emit query_failed(ixd.query, failed);
                    failed.clear();
                }

                ++ixd.query.size;
                return false;
            }, false, false
        );
        
        if (processed.size())
        { emit query_progressed(ixd.query, processed); }
        
        if (failed.size())
        { emit query_failed(ixd.query, failed); }

        return true; // TODO: need a way of determining when this sort of query is completed, irrespective of user-error
    }

    // expects mutex to be in a locked state
    // TODO: continue or cancel query in model if failed (based on user prompt)
    bool entity_data_manager::m_exec(QMutex& mutex, const std::tuple<QString, query_descriptor::query_mode> job)
    {
        using enum query_descriptor::query_mode;
        
        const QString& query_string = std::get<0>(job);
        const auto mode = std::get<1>(job);
        bool job_completed = false;

        // ################################# mutex unlock #################################
        m_working = true;
        mutex.unlock();
        setTerminationEnabled(true); // underlying APIs may hang while getting data, allow termination now
        if (mode == Fetch) // fetches take priority
        {
            job_completed = m_run_fetch_query(m_fetches.at(query_string));
        }
        else if (mode == Sort)
        {
            job_completed = m_run_sort_query(m_pending_sort.at(query_string), m_pending_sort_keys.at(query_string));
        }
        else if (mode == Read)
        {
            job_completed = m_run_read_query(m_nodes.at(query_string));
        }
        else if (mode == Read_Extended)
        {
            job_completed = m_run_extended_read_query(m_extended_reads.at(query_string), m_extended_read_positions.at(query_string));
        }
        else if (mode == Write)
        {
            // FIXME: we need to know the query type in order to avoid checking each map for a matching query
            if (m_indexed_writes.contains(query_string))
            {
                job_completed = m_run_indexed_write_query(m_indexed_writes.at(query_string));
            }
        }
        // ################################# mutex lock #################################
        setTerminationEnabled(false); // we're about to grab the mutex, disable termination now
        mutex.lock();

        if (job_completed)
        {
            query_descriptor qd;
            if (mode == Read)
            {
                qd = m_nodes[query_string].descriptor;
                m_nodes.erase(query_string);
                m_invalidated.erase(query_string);
                m_sources.erase(query_string);
            }
            else if (mode == Read_Extended)
            {
                m_extended_reads.erase(query_string);
                m_extended_read_positions.erase(query_string);
            }
            else if (mode == Sort)
            {
                m_pending_sort.erase(query_string);
                m_pending_sort_keys.erase(query_string);
            }
            else if (mode == Fetch)
            {
                qd = m_fetches[query_string].query;
                m_fetches.erase(query_string);
            }
            else if (mode == Write)
            {
                // FIXME: need to identify what kind of write query
                if (m_indexed_writes.contains(query_string))
                {
                    qd = m_indexed_writes.at(query_string).query;
                    m_indexed_writes.erase(query_string);
                }
            }

            if (!(mode == Sort || mode == Read_Extended))
            { emit query_done(std::move(qd)); }

            m_queries.pop_front();
        }

        const bool work_pending = !m_queries.empty();

        // ####### mutating of internal state not allowed past this point #######
        m_working = false;
        
        if (isInterruptionRequested())
        { return true; }
        
        m_no_work_being_performed.wakeAll(); // if there was a waiter, mutex is returned to this thread by waiter in a locked state
        if (work_pending && m_do_interrupt.test())
        {
            m_do_interrupt.clear();

            // wait to be told to continue working
            qDebug() << "entity_data_manager: wait with work still pending";
            m_continue_query.wait(&m_mutex);
        }
        return false;
    }

    void entity_data_manager::m_adjust_count(query_descriptor& qd, const reflexive_entity& e, bool increment)
    {
        qd.adjust(e, increment);
    }

    // for filesystem-based fetch queries, the parent dir of new_id is the same as qd.query_string
    // TODO: if update_entity fails, we should undo the rename
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
            e = m_make_item(info.absoluteDir().path(), new_id, info, sz::entity_type::File, is_indexed, true);
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
            e = m_make_item(info.absoluteDir().path(), dst_id, info, sz::entity_type::File, is_indexed, true);
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
            e = m_make_item(info.absoluteDir().path(), target, info, sz::entity_type::File, is_indexed, true);
        }
        return e;
    }

    bool entity_data_manager::m_remove_file_entity(const QString& id, bool is_indexed)
    {
        const QFileInfo info(id);
        const bool removed = (info.isFile() || info.isSymLink()) ? QFile::remove(id) : QDir(id).removeRecursively();

        // TODO: delete_entity should return a bool indicating success
        if (is_indexed)
        {
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
            qDebug() << "entity_datamanager: notify for update: " << path;
            const QFileInfo info(path);
            const QString dir = info.isDir() ? path : info.canonicalPath();
            emit query_needs_update(dir);
        };

        connect(watcher, &QFileSystemWatcher::directoryChanged, this, notify_for_update);
        connect(watcher, &QFileSystemWatcher::fileChanged, this, notify_for_update);
        return watcher;
    }

} // namespace rin
