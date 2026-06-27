// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <string_view>
#include <functional>
#include <algorithm>
#include <QtCore/QDir>
#include <QtCore/QMimeData>
#include <QtCore/QTimerEvent>
#include <QtWidgets/QStyle>
#include <QtWidgets/QListView>
#include <QtGui/QGuiApplication>
#include <QtGui/QPalette>
#include <QtGui/QImageReader>
#include <suzuri/utility/string.hpp>
#include "entitymodel.hpp"
#include "thumbnailmanager.hpp"

namespace rin
{
    static constexpr size_t s_default_batchsize = 10000;
    static constexpr size_t s_max_in_memory = 100000;

    // TODO: enum reflection
    using enum entity_attribute_type;
    static constexpr std::array<std::tuple<entity_attribute_type, const char*>, 10> s_field_names = {{
        {Name, "Name"}, 
        {Size, "Size"}, 
        {Modified, "Modified"}, {Created, "Created"}, {Accessed, "Accessed"}, 
        {File_Type, "Type"}, 
        {Rating, "Rating"}, {Tags, "Tags"}, {Comments, "Comments"}, {Description, "Description"}
    }};

    entity_model::entity_model(QObject* parent, const std::filesystem::path& database_path) :
        QAbstractItemModel(parent), 
        m_dataman(std::make_unique<entity_data_manager>(database_path)), m_database_path(database_path),
        m_historyman(), m_thumbman(),
        m_tree(),
        m_thumbnails(), m_items_pending_update(), m_field_names(),
        m_pending_queries(), m_delayed_sorts(), m_pending_query_timer(), m_delayed_sort_timer(),
        m_root(std::nullopt), m_sort_column(std::nullopt), m_sort_order(std::nullopt),
        m_thumbnail_target_size(256, 256),
        m_batchsize(100),
        m_readonly(true)
    {
        auto* thumbman = new thumbnail_manager;
        thumbman->moveToThread(&m_thumbman);
        connect(&m_thumbman, &QThread::finished, thumbman, &QObject::deleteLater);
        connect(this, &entity_model::need_thumbnails, thumbman, &thumbnail_manager::generate_thumbnails);
        connect(thumbman, &thumbnail_manager::thumbnails_generated, this, &entity_model::set_thumbnails);
        m_thumbman.start();

        auto* dataman = m_dataman.get();
        connect(
            dataman,
            qOverload<const entity_tree_node&, int, int>(&entity_data_manager::data_available),
            this,
            qOverload<const entity_tree_node&, int, int>(&entity_model::insert)
        );
        connect(
            dataman,
            qOverload<const query_descriptor&, const std::vector<reflexive_entity>&>(&entity_data_manager::data_available),
            this,
            qOverload<const query_descriptor&, const std::vector<reflexive_entity>&>(&entity_model::insert)
        );
        connect(dataman, &entity_data_manager::data_sorted, this, &entity_model::copy);
        connect(dataman, &entity_data_manager::data_removed, this, qOverload<const query_descriptor&, const std::vector<std::tuple<QString, int>>&>(&entity_model::remove));
        connect(
            dataman, qOverload<const query_descriptor&, std::vector<reflexive_entity>>(&entity_data_manager::update_available),
            this, qOverload<const query_descriptor&, std::vector<reflexive_entity>>(&entity_model::update)
        );
        connect(
            dataman, qOverload<const entity_tree_node&, int, int>(&entity_data_manager::update_available),
            this, qOverload<const entity_tree_node&, int, int>(&entity_model::update)
        );
        connect(dataman, &entity_data_manager::query_needs_update, this, qOverload<const QString&>(&entity_model::invalidate));
        connect(dataman, &entity_data_manager::query_progressed, this, &entity_model::handle_query_progress);
        connect(dataman, &entity_data_manager::query_done, this, &entity_model::finalize_query);
        connect(dataman, &entity_data_manager::fetch_progressed, this, &entity_model::handle_fetch_progress);
        connect(dataman, &entity_data_manager::fetch_failed, this, &entity_model::handle_fetch_failure);

        m_dataman->set_watcher_enabled(true);

        // TODO: better way of doing this
        m_field_names = std::vector<std::tuple<entity_attribute_type, QString>>{
            s_field_names[0],
            s_field_names[1],
            s_field_names[5],
            s_field_names[2]
        };
    }

    entity_model::~entity_model()
    {
        m_dataman->request_abort();
        if (!m_dataman->wait(1000))
        {
            auto* dataman = m_dataman.release();
            dataman->deleteLater();
        }

        m_thumbman.quit();
        m_thumbman.wait();   
    }

    void entity_model::set_batchsize(size_t size)
    {
        m_dataman->set_batchsize(size);
    }

    void entity_model::set_readonly(bool readonly)
    {
        m_readonly = readonly;
    }

    void entity_model::set_database(const std::filesystem::path& path)
    {
        // TODO: mark all nodes for rescan
        m_dataman->clear();
        m_dataman->set_database(path);
    }

    // root queries with no preexisting item entry have a -1 row value in the returned index
    QModelIndex entity_model::query(const QString& text)
    {
        if (text.isEmpty())
        { return QModelIndex(); }
        
        if (m_root)
        { m_deactivate(*m_root); }

        if (m_tree.contains(text))
        {
            qDebug() << "entity_model::query(): do requery for " << text;
            invalidate(text);
            m_root = m_tree[text].key;
            m_historyman.step_forward_with(text);
            return m_index_for_querytext(text);
        }

        int parent_key = 0; // 0 is never a valid key
        int i = -1;
        if (!m_tree.empty())
        {
            if (QFileInfo info(text); info.exists())
            {
                if (const QString dir_path = info.absoluteDir().canonicalPath(); m_tree.contains(dir_path))
                {
                    parent_key = m_tree.key_for_querytext(dir_path);
                    i = m_tree.index_of(parent_key, info.fileName());
                }
            }
            // TODO: parent/child related indexed queries
        }

        m_create_node(text, parent_key, i);
        m_root = m_tree[text].key;
        m_historyman.step_forward_with(text);
        return createIndex(i, 0, m_tree.contains(parent_key) ? parent_key : *m_root);
    }

    // create a concept query out of the entities derived from 'urls'
    QModelIndex entity_model::query(const QString& text, const QList<QUrl>& urls)
    {
        if (m_tree.contains(text))
        {
            qDebug() << "entity_model::query(): do requery for " << text;
            invalidate(text, urls);
            return m_index_for_querytext(text);
        }

        // TODO: should concept queries be inserted into the history?
        // TODO: parent/child related concept queries
        m_create_node(text, urls, 0, -1);
        return createIndex(-1, 0, m_tree[text].key);
    }

    void entity_model::tag(const QModelIndex& index, const std::vector<reflexive_entity>& tag_entities)
    {
        if (index.isValid())
        {
            auto& e = m_item_for_index(index);
            
            QString entity_id;
            if (e.has_attribute(Id))
            { entity_id = e.attribute<QString>(Id); }
            else
            { entity_id = m_id_for_index(index); }

            std::vector<std::string> tags;
            for (const auto& tag_entity : tag_entities)
            {
                // tag name is equivalent to tag entity id
                tags.push_back(tag_entity.attribute<QString>(Name).toStdString());
            }

            sz::metadata::parameters args{
                .tags = tags,
                .sidecar_keys{},
                .sidecar_split_character{},
                .sidecar_alternate_root_path{},
                .sidecar_type{},
                .use_sidecars = false,
                .check_tags = false
            };
            m_in_progress_db_writes.emplace(entity_id, m_dataman->tag(entity_id, args));
        }
    }

    void entity_model::tag(const QModelIndex& index, const sz::metadata::parameters& args)
    {
        if (index.isValid())
        {
            const QString entity_id = m_id_for_index(index);
            m_in_progress_db_writes.emplace(entity_id, m_dataman->tag(entity_id, args));
        }
    }

    void entity_model::tag(const QString& entity_id, const sz::metadata::parameters& args)
    {
        if (entity_id.size())
        { m_in_progress_db_writes.emplace(entity_id, m_dataman->tag(entity_id, args)); }
    }

    // void entity_model::query(const QList<QModelIndex>& indexes)
    // {
    //     using enum sz::entity_type;
    //     std::unordered_map<QString, std::vector<reflexive_entity>> data;
    //     for (const auto& index : indexes)
    //     {
    //         const auto& node = m_node_for_index(index);
    //         const auto& e = node[index.row()];
    //         const QString& name = e.attribute<QString>(Name);
    //         m_items_pending_update[node.descriptor.text].emplace(
    //             name,
    //             item_temp_data{
    //                     .old_name = name,
    //                     .index = index.row(),
    //                     .action = fetch_action::Ignore
    //             });
    //         data[node.descriptor.text].emplace_back(node[index.row()]);
    //     }
    //     for (const auto& [query_text, entities] : data)
    //     { m_dataman->query(m_tree[query_text].descriptor, entities); }
    // }

    sz::result<QModelIndex> entity_model::step(bool forward)
    {
        if (forward ? m_historyman.step_forward() : m_historyman.step_back())
        { return m_reset_root_query(m_historyman.current()); }
        
        return {sz::result_code::Invalid_Argument};
    }

    void entity_model::watch(const QModelIndex& index)
    {
        if (const QString id = id_for_index(index); m_tree.contains(id))
        { m_dataman->watch_path(id); }
    }

    void entity_model::unwatch(const QModelIndex& index)
    {
        if (const QString id = id_for_index(index); m_tree.contains(id))
        { m_dataman->unwatch_path(id); }
    }

    bool entity_model::remove(const QModelIndex& index, bool recycle)
    {
        if (!m_valid_index(index))
        { return false; }

        if (const QString id = m_id_for_index(index); id.size())
        {
            const int key = m_key_for_index(index);
            const QString parent_id = m_tree[key].descriptor.text;
            const auto& item = m_item_for_index(index);

            m_dataman->unwatch_path(parent_id);
            m_dataman->fetch(parent_id, {entity_model_url{
                .src = QUrl::fromLocalFile(id),
                .target_name = item.attribute<QString>(Name),
                .index = index.row()
            }}, recycle ? fetch_action::Recycle : fetch_action::Remove, false);
            return true;
        }
        return false;
    }

    void entity_model::clear_thumbnails()
    {
        // if invoked from set_thumbnail, dataChanged is emitted after determining which items need thumbnails
        // otherwise, the signal will be emitted when the model receives a new thumbnail from thumbman
        m_thumbnails.clear();
    }

    // expects all indexes to be valid
    void entity_model::request_thumbnails(const std::vector<QModelIndex>& indexes, const std::vector<QSize>& sizes)
    {
        if (sizes.size() != indexes.size())
        {
            qWarning() << "entity_model::request_thumbnails: mismatch between indexes and sizes for indexes";
        }
        std::vector<std::tuple<QString, QModelIndex, QSize>> jobs;
        std::vector<QSize> sizes_to_submit;
        
        int i = 0;
        for (const auto& index : indexes)
        {
            if (const auto& item = m_item_for_index(index); item.has_attribute(Sizehint))
            {
                jobs.emplace_back(m_id_for_index(index), index, sizes[i]);
            }
            ++i;
        }
        emit need_thumbnails(jobs);
    }

    bool entity_model::valid_index(const QModelIndex& index)
    {
        return m_valid_index(index);
    }

    bool entity_model::extended_information_loaded(const QModelIndex &index) const
    {
        using enum entity_attribute_type;
        using enum sz::entity_type;
        
        const auto& e = m_item_for_index(index);
        
        if (e.type() == File)
        { return e.has_attribute(Mime_Type); }

        return e.has_attribute(Icon);
    }

    QPixmap entity_model::thumbnail(const QModelIndex& index) const
    {
        if (index.isValid())
        {
            const auto& n = m_node_for_index(index);
            const auto& e = m_item_for_index(index);
            const auto name = e.attribute<QString>(Name);
            
            if (m_thumbnails.contains(n.key) && m_thumbnails.at(n.key).contains(name))
            { return m_thumbnails.at(n.key).at(name); }
        }
        return QPixmap();
    }

    QPixmap entity_model::thumbnail(const reflexive_entity& e) const
    {
        const int k = e.parent_key();
        const auto name = e.attribute<QString>();
        
        if (m_thumbnails.contains(k) && m_thumbnails.at(k).contains(name))
        { return m_thumbnails.at(k).at(name); }
        
        return QPixmap();
    }

    bool entity_model::has_thumbnail(const QModelIndex &index) const
    {
        if (m_valid_index(index) && index.row() != -1 && index.column() == 0)
        {
            return m_item_for_index(index).has_attribute(Sizehint);
        }
        return false;
    }

    bool entity_model::thumbnail_loaded(const QModelIndex& index) const
    {
        const int key = m_key_for_index(index);
        return m_thumbnails.contains(key) && m_thumbnails.at(key).contains(m_tree[key].positions[index.row()]);
    }

    QModelIndex entity_model::root_index() const
    {
        if (m_root)
        { return m_index_for_querytext(m_id_for_key(*m_root)); }
        return QModelIndex();
    }

    QString entity_model::root_query() const
    {
        if (m_root)
        { return m_node_for_key(*m_root).descriptor.full_text(); }

        return QString();
    }

    QString entity_model::id_for_index(const QModelIndex& index) const
    {
        return m_id_for_index(index);
    }

    const reflexive_entity& entity_model::at(const QModelIndex& index)
    {
        reflexive_entity& e = m_tree[m_key_for_index(index)][index.row()];
        m_refresh_entity(e);
        return std::as_const(e);
    }

    entity_model::item_properties entity_model::properties(const QModelIndex& parent) const
    {
        if (const QString id = m_id_for_index(parent); m_tree.contains(id))
        {
            return m_tree[id].descriptor;
        }
        return item_properties();
    }

    // returns true if the tag exists in the database
    bool entity_model::entity_exists(const std::string& entity_id)
    {
        sz::entity_database db(m_database_path, true);
        const bool exists = db.entity_exists(entity_id);
        return exists;
    }

    QModelIndex entity_model::index(int row, int column, const QModelIndex& parent) const
    {
        if (row >= rowCount(parent) || column >= columnCount(parent) || column < 0) // -1 row, 0 col refers to a node
        { return QModelIndex(); }

        const auto& n = m_node_for_index(parent);
        if (const int k = parent.row() == -1 ? n.key : m_tree[n.key][parent.row()].key(); m_tree.contains(k))
        { return createIndex(row, column, k); }
        
        return QModelIndex();
    }
    
    // FIXME: this doesn't work if root is not set
    QModelIndex entity_model::parent(const QModelIndex& child) const
    {
        const auto& node = m_node_for_index(child);
        return createIndex(node.index, 0, m_tree.contains(node.parent) ? node.parent : node.key);
    }

    int entity_model::rowCount(const QModelIndex& parent) const
    {
        int count = 0;
        if (m_root && (!m_valid_index(parent)))
        {
            count = m_node_for_key(*m_root).size();
        }
        else if (const QString id = m_id_for_index(parent); m_tree.contains(id))
        {
            count = m_tree[id].size();
        }
        return count;
    }

    int entity_model::columnCount(const QModelIndex& parent) const
    {
        Q_UNUSED(parent);
        return static_cast<int>(m_field_names.size());
    }

    bool entity_model::hasChildren(const QModelIndex& parent) const
    {
        if (m_root && (!m_valid_index(parent) || parent.row() == -1))
        {
            return m_node_for_key(*m_root).descriptor.size > 0;
        }
        
        return m_item_for_index(parent).has_children();
    }

    QVariant entity_model::data(const QModelIndex& index, int role) const
    {
        if (!index.isValid())
        { return QVariant(); }

        const auto& item = m_node_for_valid_index(index)[index.row()];
        const int col = index.column();
        
        switch (role)
        {
        case Qt::DisplayRole:
        {
            // m_field_names should only point to string-based attributes
            if (const entity_attribute_type attr = std::get<0>(m_field_names[col]); item.has_attribute(attr))
            {
                // TODO: stronger run-time validation of attribute type
                return item.attribute<QString>(attr);
            }
            break;
        }

        case Qt::EditRole:
        { return item.attribute<QString>(Name); }

        case Qt::DecorationRole:
        {
            const int key = item.parent_key();
            if (m_thumbnails.contains(key) && m_thumbnails.at(key).contains(item.attribute<QString>(Name)))
            {
                return m_thumbnail_for_index(index);
            }
            else if (item.has_attribute(Sizehint))
            {
                auto* this_ = const_cast<entity_model*>(this);
                emit this_->need_thumbnails({{m_id_for_index(index), QModelIndex(index), rin::fit_under(m_thumbnail_target_size, item.attribute<QSize>(Sizehint))}});
            }

            if (item.has_attribute(Icon))
            { return item.attribute<QIcon>(Icon); }

            if (item.has_attribute(Mime_Type))
            {
                if (const auto& mime = item.attribute<QMimeType>(); m_icon_cache.contains(mime))
                { return m_icon_cache[mime]; }
            }

            if (item.type() == sz::entity_type::File)
            { return m_icon_provider.icon(item.attribute<QFileInfo>()); }

            break;
        }

        case Qt::SizeHintRole:
        {
            if (item.has_attribute(Sizehint))
            { return item.attribute<QSize>(Sizehint); }
            
            // delegate always expects to get a QSize when querying the size hint role
            return QSize();
        }
        
        default:
        { break; }
        }

        return QVariant();
    }

    bool entity_model::setData(const QModelIndex& index, const QVariant& value, int role)
    {
        // TODO: depending on the entity, we would need to query a filesystem write or a database write
        using enum entity_tree_node_state;
        if (value.isNull() || !m_valid_index(index))
        { return false; }

        const auto& item = m_item_for_index(index);
        const QString query_text = m_tree[item.parent_key()].descriptor.text;
        
        if (role == Qt::EditRole && item.type() == sz::entity_type::File)
        {
            m_tree[item.parent_key()].state = Updating;
            const QString old_name = item.attribute<QString>(Name);
            const QString new_name = value.toString();
            const QString parent_dir_path = item.attribute<QFileInfo>(File_Info).absoluteDir().canonicalPath() + QDir::separator();
            const QUrl url = QUrl::fromLocalFile(parent_dir_path + old_name);
            const QString new_path = parent_dir_path + new_name;
            
            qDebug() << "entity_model::setData: set value: " << value << ", to index: " << index << ", url used: " << url;
            
            const auto action = fetch_action::Move;

            if (m_tree.contains(item.key()))
            {
                m_tree.traverse(item.parent_key(), [this](const entity_tree_node& node, int depth) -> bool
                {
                    Q_UNUSED(depth);
                    m_dataman->unwatch_path(node.descriptor.text);
                    return false;
                });
            }
            else
            {
                m_dataman->unwatch_path(query_text);
            }
            m_dataman->fetch(
                query_text,
                {entity_model_url{
                    .src = url,
                    .target_name = new_name,
                    .index = index.row()
                }},
                action, true
            );
            m_items_pending_update[m_tree[item.parent_key()].descriptor.full_text()].emplace(new_name, item_temp_data{
                .old_name = old_name,
                .index = index.row(),
                .action = action
            });
            
            return true; // update() will emit dataChanged if the rename was successful
        }
        // TODO: other entity types
        return false;
    }

    QVariant entity_model::headerData(int section, Qt::Orientation orientation, int role) const
    {
        if (
            std::cmp_greater_equal(section, m_field_names.size()) ||
            (section < 0) ||
            (orientation == Qt::Orientation::Vertical) ||
            (role != Qt::DisplayRole)
        )
        { return QVariant(); }

        return std::get<1>(m_field_names[section]);
    }

    // TODO: When reimplementing this function, the headerDataChanged() signal must be emitted explicitly.
    bool entity_model::setHeaderData(int section, Qt::Orientation orientation, const QVariant& value, int role)
    {
        if (
            std::cmp_greater_equal(section, m_field_names.size()) ||
            (section < 0) ||
            (orientation == Qt::Orientation::Vertical) ||
            (role != Qt::EditRole) ||
            (value.userType() != QMetaType::QString)
        )
        { return false; }

        m_field_names[section] = {entity_attribute_type(section), value.toString()}; // ???
        return true;
    }

    // TODO: figure out what mime types to use when the query is not backed by the filesystem directly
    QStringList entity_model::mimeTypes() const
    {
        using namespace Qt::StringLiterals;
        return QStringList("text/uri-list"_L1);
    }

    // TODO: figure out what to do for entities not backed by the filesystem directly
    // referenced from: qfilesystemmodel.cpp, line 1206
    // this function assumes invalid indexes are never passed to it
    QMimeData* entity_model::mimeData(const QModelIndexList& indexes) const
    {
        using enum entity_attribute_type;
        using enum sz::entity_type;
        QList<QUrl> urls;
        for (const auto& index : indexes)
        {
            const auto& item = m_item_for_index(index);
            if ((item.type() == File) && std::get<0>(m_field_names[index.column()]) == Name)
            {
                urls << QUrl::fromLocalFile(m_absolute_path_for_file_entity(index));
            }
        }
        QMimeData* data = new QMimeData();
        data->setUrls(urls);
        return data; // returning a pointer to fresh off the heap memory... here's hoping the caller remembers to delete
    }

    bool entity_model::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) const
    {
        using enum query_descriptor::query_type;
        Q_UNUSED(row);
        Q_UNUSED(column);

        if (m_node_for_index(parent).descriptor.type == Indexed)
        { return false; } // TODO

        bool can_drop = false;
        if (!(action & supportedDropActions()))
        { return false; }

        const auto types = mimeTypes();
        for (const auto& type : types)
        {
            if (data->hasFormat(type))
            { can_drop = true; }
        }

        return can_drop && hasChildren(parent); // TODO
    }

    // TODO: handle drops that occur on indexed queries
    // TODO: handle cases where an item is indexed
    // FIXME: drops that occur for items that don't yet have a node don't have their size attribute updated
    bool entity_model::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent)
    {
        using enum query_descriptor::query_type;
        Q_UNUSED(row);
        Q_UNUSED(column);
        qDebug() << "entity_model::dropMimeData: parent: " << parent << ", row: " << row << ", col: " << column << ", action: " << action;
        
        bool ok = false;
        
        if (m_readonly)
        { return ok; }

        if (flags(parent) & Qt::ItemIsDropEnabled)
        {
            ok = true;
            const QString id = m_id_for_index(parent);
            m_dataman->unwatch_path(id); // otherwise the path will be invalidated when the fetch starts

            std::vector<entity_model_url> urls;
            for (const auto& qurl : data->urls())
            {
                urls.emplace_back(entity_model_url{
                    .src = qurl,
                    .target_name = QFileInfo(qurl.toLocalFile()).fileName(),
                    .index = -1 // non-update fetches will trigger an insert()
                });
            }
            m_dataman->fetch(id, std::move(urls), fetch_action{action});
        }
        else
        {
            qDebug() << "entity_model::dropMimeData: not an item that accepts drops";
            ok = false;
        }
        
        return ok;
    }

    Qt::DropActions entity_model::supportedDropActions() const
    {
        return Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
    }

    // TODO: this might have to change if the root query represents a concept entity
    Qt::DropActions entity_model::supportedDragActions() const
    {
        return supportedDropActions();
    }

    // TODO (entity_data_manager): concurrency for query jobs
    // this function has no effect if given a fully invalid index
    void entity_model::fetchMore(const QModelIndex& parent)
    {
        qDebug() << "entity_model: attempting to fetch more for " << parent;

        if (const QString id = m_id_for_index(parent); m_tree.contains(id))
        {
            m_dataman->continue_query(m_tree[id].descriptor);
        }
        else if (id.size())
        {
            // create a new node and query if the item has children
            m_create_node(id, m_key_for_index(parent), parent.row());
        }
    }

    bool entity_model::canFetchMore(const QModelIndex& parent) const
    {
        if (const QString id = m_id_for_index(parent); id.size())
        {   
            if (m_tree.contains(id))
            {
                const auto& node = m_tree[id];
                return std::cmp_less(node.size(), node.descriptor.size) || node.descriptor.size == -1;
            }
            else if (const auto& item = m_item_for_index(parent); item.has_children())
            {
                // a node doesn't exist because fetchMore was not called for this item yet
                return true;
            }
        }
        
        if (m_root)
        {
            const auto& root = m_node_for_key(*m_root);
            return std::cmp_less(root.size(), root.descriptor.size) || root.descriptor.size == -1;
        }

        return false;
    }

    Qt::ItemFlags entity_model::flags(const QModelIndex& index) const
    {
        using enum sz::entity_type;
        Qt::ItemFlags flags = index.isValid() ? Qt::ItemIsSelectable | Qt::ItemIsEnabled : Qt::ItemFlags{};

        if (!index.isValid())
        {
            flags |= Qt::ItemIsDropEnabled;
            return flags;
        }

        if (const reflexive_entity& item = m_node_for_valid_index(index)[index.row()]; item.type() == File)
        {
            flags |= Qt::ItemIsDragEnabled;
            const QFileInfo& info = item.attribute<QFileInfo>(File_Info);

            // file entities never hold insets
            // they cannot be used to describe another entity, only concept entities can use files to describe other entities
            if (!info.isDir())
            { flags |= Qt::ItemNeverHasChildren; }

            if (m_readonly)
            { return flags; }

            if (info.permissions() & QFile::WriteUser)
            {
                flags |= Qt::ItemIsEditable;
            
                if (info.isDir())
                { flags |= Qt::ItemIsDropEnabled; }
            }
        }
        // TODO: other entities

        return flags;
    }

    // since we cannot take a key argument here, the entire model will be sorted starting at the currently set root
    void entity_model::sort(int column, Qt::SortOrder order)
    {
        set_sort_preference(column, order);
        if (m_root)
        {
            qDebug() << "entity_model: sort tree: column: " << column << ", order: " << order;
            m_tree.traverse(*m_root, [&](const entity_tree_node& node, int depth) -> bool
            {
                Q_UNUSED(depth);
                m_sort(node.key, column, order);
                return false;
            });
        }
    }

    // TODO
    QModelIndex entity_model::buddy(const QModelIndex& index) const
    {
        return QAbstractItemModel::buddy(index);
    }

    // TODO
    QModelIndexList entity_model::match(const QModelIndex& start, int role, const QVariant& value, int hits, Qt::MatchFlags flags) const
    {
        return QAbstractItemModel::match(start, role, value, hits, flags);
    }

    QSize entity_model::span(const QModelIndex& index) const
    {
        Q_UNUSED(index);
        return QSize(1, 1);
    }

    // TODO: insert our custom roles
    QHash<int, QByteArray> entity_model::roleNames() const
    {
        return QHash<int, QByteArray>{
            { Qt::DisplayRole,    "display"    },
            { Qt::DecorationRole, "decoration" },
            { Qt::EditRole,       "edit"       },
            { Qt::ToolTipRole,    "toolTip"    },
            { Qt::StatusTipRole,  "statusTip"  },
            { Qt::WhatsThisRole,  "whatsThis"  }
        };
    }

    void entity_model::set_thumbnail_target_size(const QSize& size)
    {
        if (m_thumbnail_target_size == size)
        { return; }

        m_thumbnail_target_size = size;
        clear_thumbnails();
    }

    void entity_model::set_sort_preference(int column, Qt::SortOrder order)
    {
        qDebug() << "entity_model: set sort preference: " << column << ", order: " << order;
        m_sort_column = column;
        m_sort_order = order;
    }

    void entity_model::timerEvent(QTimerEvent* e)
    {
        if (e->id() == m_pending_query_timer.id())
        {
            // FIXME
            // const QString& text = m_pending_queries.front();
            // if (m_tree.contains(text))
            // {
            //     m_dataman->query(text, m_tree[text]);
            // }
            // m_remove_delayed_query(text);
        }
        else if (e->id() == m_delayed_sort_timer.id())
        {
            if (m_sort_column && m_sort_order)
            {
                m_sort(m_delayed_sorts.front(), *m_sort_column, *m_sort_order);
                m_remove_delayed_sort();
            }
            else
            { m_interrupt_delayed_sort(); }
        }
        // else if (e->id() == m_delayed_info_fetch_timer.id())
        // {
        //     using enum sz::entity_type;
        //     // assumption is k exists in tree and i is a valid index into the corresponding node
        //     auto [k, i] = m_delayed_info_fetches.front();
        //     auto& node = m_tree[k];
        //     std::vector<reflexive_entity> data;
        //     data.reserve(m_batchsize);
        //     int j = i;
        //     for ( ; j < std::min(node.size(), i + m_batchsize); ++j)
        //     {
        //         const auto& e = node[j];
        //         if (!extended_information_loaded(m_index_for_item(e, j)))
        //         {
        //             const QString& name = e.attribute<QString>(Name);
        //             m_items_pending_update[node.descriptor.text].emplace(
        //                 name,
        //                 item_temp_data{
        //                     .old_name = name,
        //                     .index = j,
        //                     .action = fetch_action::Ignore
        //                 }
        //             );
        //             data.emplace_back(e);
        //         }
        //     }
        //     m_dataman->query(node.descriptor, data);
        //     m_remove_delayed_info_fetch();
        // }
        QObject::timerEvent(e);
    }

    // for read/rescan
    void entity_model::insert(const entity_tree_node& updated_node, int start, int end)
    {
        if (const int k = updated_node.key; start <= end && m_tree.contains(k))
        { m_insert_entities(m_index_for_querytext(updated_node.descriptor.full_text()), k, start, end, updated_node, true); }
    }

    // for fetch queries and query_descriptor size updates
    void entity_model::insert(const query_descriptor& qd, const std::vector<reflexive_entity>& data)
    {
        using enum query_descriptor::query_mode;
        if (m_tree.contains(qd))
        {
            const int key = m_tree.key_for_descriptor(qd);
            auto& node = m_tree[key];
            if (qd.mode == Read && data.empty())
            {
                node.descriptor = qd;
                node.reserve(static_cast<size_t>(qd.size));
                return; // fetch queries do not emit signals unless they have data to send
            }
            if (qd.mode == Fetch)
            {
                node.descriptor += qd;
                node.descriptor.size += data.size();
            }
            const int start = node.size();
            m_insert_entities(m_index_for_querytext(qd.full_text()), key, start, start + static_cast<int>(data.size()) - 1, data, false);
        }
    }

    void entity_model::remove(const query_descriptor& qd, const std::vector<std::tuple<QString, int>>& to_remove)
    {
        if (m_tree.contains(qd.full_text()) && std::cmp_less_equal(to_remove.size(), m_tree[qd.full_text()].size()))
        {
            qDebug() << "entity_model: begin remove for: " << qd.full_text() << ", items to remove: " << to_remove.size();
            const int k = m_tree.key_for_querytext(qd.full_text());
            m_tree[k].descriptor = qd;
            for (const auto& t : to_remove)
            {
                const int i = std::get<1>(t);
                m_purge_item(k, m_tree[k][i].key(), i);
            }
        }
    }

    // expected to be called when the filesystem watcher inside dataman detects a change inside a watched dir
    // any isolated nodes as a result of the invalidate are taken care of in remove() when dataman detects isolated items
    // unwatching the path doesn't seem to be ideal here, as the path could change inbetween invalidates.
    // every time the watcher notifies for update, we re-invalidate the node to ensure we always have the most up-to-date version of source in dataman
    // TODO: ensure query_text is the full query text
    void entity_model::invalidate(const QString& query_text)
    {
        if (m_tree.contains(query_text))
        {
            entity_tree_node& node = m_tree[query_text];
            m_do_invalidate(node);
            
            // note that dataman does not sort nodes that are invalidated, we pass the sort_key/sort_order in order to preserve the information
            // since dataman will overwrite the descriptor for the node
            m_dataman->query(query_text, m_tree, node.index, node.parent, node.descriptor.sort_key, node.descriptor.sort_order);
        }
    }

    void entity_model::invalidate(const QString& query_text, const QList<QUrl>& urls)
    {
        if (m_tree.contains(query_text))
        {
            entity_tree_node& node = m_tree[query_text];
            m_do_invalidate(node);
            m_dataman->query(query_text, m_tree, urls, node.index, node.parent);
        }
    }

    // used for non-move renames, tag updates/adding
    // TODO: other entity types
    void entity_model::update(const query_descriptor& qd, std::vector<reflexive_entity> data)
    {
        if (m_tree.contains(qd))
        {
            qDebug() << "entity_model: update query: " << qd.full_text();
            const int key = m_tree.key_for_descriptor(qd);
            for (auto& incoming : data)
            {
                if (const QString name = incoming.attribute<QString>(Name); m_items_pending_update[qd.full_text()].contains(name))
                {
                    const item_temp_data temp = m_items_pending_update[qd.full_text()][name];
                    m_items_pending_update[qd.full_text()].erase(name);
                    
                    if (incoming.has_attribute(Mime_Type))
                    { incoming.set_attribute(Icon, m_get_icon(incoming)); }
                    
                    m_tree.reseat(key, temp.index, std::move(incoming));
                    const QModelIndex index = m_index_for_item(m_tree[key][temp.index], temp.index);
                    qDebug() << "entity_model::update: index updated: " << index;
                    emit dataChanged(index, index);
                }
            }
        }
    }

    // used for extended info queries
    void entity_model::update(const entity_tree_node& updated_node, int start, int end)
    {
        if (m_tree.contains(updated_node.key))
        {
            qDebug() << "entity_model: update node: " << updated_node.descriptor.full_text() << ", start: " << start << ", end: " << end;
            auto& node = m_tree[updated_node.key];
            for (int i = start; i <= end; ++i)
            {
                node[i] = updated_node[i];
                auto& e = node[i];
                e.set_attribute(Icon, m_get_icon(e));
            }
            node.descriptor.processed_extended = updated_node.descriptor.processed_extended;
            m_dataman->continue_query(); // updates occur after finalize. continue any work still pending now
            emit dataChanged(m_index_for_item(node[start], start), m_index_for_item(node[end], end));
        }
    }

    void entity_model::copy(const entity_tree_node& updated_node, const std::vector<node_index_change>& updated_key_indexes)
    {
        using enum query_descriptor::query_mode;
        if (m_tree.contains(updated_node.key))
        {
            const int key = updated_node.key;
            auto& node = m_tree[key];
            if (updated_node.descriptor.mode == Sort)
            {
                emit sort_starting();
                QList<QModelIndex> old_indexes;
                QList<QModelIndex> new_indexes;
                for (const auto& [k, old_index, new_index] : updated_key_indexes)
                {
                    const auto& item = node[old_index];
                    old_indexes << m_index_for_item(item, old_index);
                    m_tree[k].index = new_index;
                    new_indexes << m_index_for_item(item, new_index);
                }
                node.positions = updated_node.positions; // sorted
                node.descriptor.mode = Read;

                if (new_indexes.size())
                { changePersistentIndexList(old_indexes, new_indexes); }

                m_dataman->continue_query(); // a read query may still be pending, especially if the sort is the result of an invalidate
                emit sort_finished();
            }
            else
            {
                qDebug() << "entity_model: copy() called for node: " << updated_node.descriptor.full_text() << ", no signals will be emitted.";
                node = updated_node;
            }
        }
    }

    void entity_model::set_thumbnails(const std::vector<std::tuple<QString, QModelIndex, QPixmap>>& data)
    {
        for (const auto& [id, idx, thumb] : data)
        {
            const auto& item = m_item_for_index(idx);
            if (
                (item.type() == sz::entity_type::File && item.attribute<QFileInfo>(File_Info).absoluteFilePath() == id) ||
                (item.attribute<QString>(Name) == id)
            )
            {
                const int key = m_key_for_index(idx);
                const int i = idx.row();
                m_thumbnails[key][m_tree[key].positions[i]] = thumb;
                emit dataChanged(idx, idx, {Qt::DecorationRole});
            }
        }
    }

    void entity_model::finalize_query(query_descriptor qd)
    {
        using enum query_descriptor::query_mode;
        using enum entity_tree_node_state;
        if (m_tree.contains(qd))
        {
            const int key = m_tree.key_for_descriptor(qd);
            auto& node = m_tree[key];
            m_dataman->watch_path(qd.text); // paths are unwatched prior to submitting fetch queries
            if (qd.mode == Read)
            {
                node.descriptor = qd;
                if (node.state != Invalidated)
                { m_dataman->query(node); } // extended read query
            }
            else if (qd.mode == Fetch)
            {
                if (node.descriptor.sort_key)
                { m_async_sort(key, *node.descriptor.sort_key, *node.descriptor.sort_order); }
            }
            m_update_size_attribute_for_item_representing_node(key);
            node.state = Valid;
            m_dataman->continue_query();
            qDebug() << "[entity_model] query finished: " << qd.full_text() << ", size: " << qd.size << ", size of node: " << m_tree[key].items.size();
            emit query_done(m_index_for_querytext(qd.full_text()));
        }
        else if (m_in_progress_db_writes.contains(qd.text))
        {
            qDebug() << "[entity_model] write completed for: " << qd.text;
            emit query_done(qd.text);
            m_in_progress_db_writes.erase(qd.text);
        }
    }

    void entity_model::handle_query_progress(query_descriptor qd, QList<std::tuple<QString, int>> processed)
    {
        if (m_in_progress_db_writes.contains(qd.text))
        {
            m_in_progress_db_writes[qd.text].query = qd;
            emit query_progressed(qd.text, processed);
        }
    }

    void entity_model::handle_query_failure(query_descriptor qd, QList<std::tuple<QString, QString>> failed)
    {
        if (m_in_progress_db_writes.contains(qd.text))
        {
            m_in_progress_db_writes[qd.text].query = qd;
            emit query_failed(qd.text, failed);
        }
    }

    // FIXME: not ideal to be calling index_of() per item here
    void entity_model::handle_fetch_progress(fetch_descriptor fd, std::vector<entity_model_url> urls_processed)
    {
        using enum fetch_action;
        if (m_tree.contains(fd.query))
        {
            const int k = m_tree.key_for_descriptor(fd.query);
            if (fd.action == Remove || fd.action == Recycle)
            {
                for (const auto& url : urls_processed)
                {
                    auto& node = m_tree[k];
                    if (const int idx = node.index_of(url.target_name); idx > -1)
                    {
                        --node.descriptor.size;
                        node.descriptor.adjust(node[idx], false);
                        m_purge_item(k, node[idx].key(), idx);
                    }
                }
            }
        }
    }

    void entity_model::handle_fetch_failure(fetch_descriptor fd, std::vector<entity_model_url> urls)
    {
        qDebug() << "fetch failed for urls (action was " << std::to_underlying(fd.action) << "):";
        const QString target = fd.query.text;
        m_dataman->watch_path(target);
        m_dataman->cancel_query(fd.query); // TODO: only cancel on user request
        emit fetch_failed(target, urls, fd.action);
    }

    sz::result<entity_tree_node_state> entity_model::node_state(const QModelIndex& parent) const
    {
        const auto& node = m_node_for_index(parent);
        const int index = parent.row();
        const int key = index > -1 && std::cmp_less(index, node.items.size()) ? node[index].key() : 0;
        if (m_tree.contains(key))
        {
            qDebug() << "entity_model: return state for: " << m_tree[key].descriptor.full_text() << ", index was: " << parent;
            return m_tree[key].state;
        }
        else if (m_root)
        {
            qDebug() << "entity_model: return state for root: " << m_tree[*m_root].descriptor.full_text() << ", index was: " << parent;

        }
        return sz::common_error(sz::result_code::Invalid_Argument);
    }

    // TODO: support for other entity types
    inline QIcon entity_model::m_get_icon(const reflexive_entity& e)
    {
        if (e.type() == sz::entity_type::File)
        {
            const auto& mime = e.attribute<QMimeType>();
            if (m_icon_cache.contains(mime))
            { return m_icon_cache[mime]; }

            const QIcon icon = m_icon_provider.icon(e.attribute<QFileInfo>());    
            m_icon_cache[mime] = icon;
            return icon;
        }
        return QIcon();
    }

    inline void entity_model::m_refresh_entity(reflexive_entity& e)
    {
        const int parent = e.parent_key();
        const QString parent_id = m_tree[parent].descriptor.text;
        if (e.has_attribute(Parent_Id))
        {
            e.attribute<QString>(Parent_Id) = parent_id;
        }

        if (e.type() == sz::entity_type::File && m_tree[parent].descriptor.type == query_descriptor::query_type::Filesystem)
        {
            QFileInfo& info = e.attribute<QFileInfo>(File_Info);
            info.setFile(parent_id + QDir::separator() + e.attribute<QString>(Name));
            info.refresh();
        }
    }

    // assumes key exists in the tree
    inline void entity_model::m_update_size_attribute_for_item_representing_node(int key)
    {
        const int pk = m_tree[key].parent;
        const int i = m_tree[key].index;
        if (m_tree.contains(pk) && i > -1) // if this node occurs as an item in another node
        {
            qDebug() << "entity_model: update size attribute for item representing node: " << m_tree[key].descriptor.full_text();

            auto& e = m_tree[pk][i];
            const QString size_string = QLocale::system().toString(m_tree[key].descriptor.size) + " items";
            
            if (!e.has_attribute(Size))
            { e.set_attribute(Size, size_string); }
            else
            { e.attribute<QString>(Size) = size_string; }

            const QModelIndex idx = m_index_for_item(e, i);
            emit dataChanged(idx, idx);
        }
    }

    inline int entity_model::m_key_for_index(const QModelIndex& index) const
    {
        return static_cast<int>(index.internalId());
    }

    // field_names are not cleared here
    // this function would be called by resetInternalData(), which is called by QAbstractItemModel inside endResetModel()
    inline void entity_model::m_clear()
    {
        clear_thumbnails();
        m_dataman->clear();
        m_historyman.clear();
        m_pending_queries.clear();
        m_pending_query_timer.stop();
        m_delayed_sort_timer.stop();
        m_tree.clear();
        m_items_pending_update.clear();
        m_root = std::nullopt;
    }

    // cancels any pending queries for the node pointed to by key and unwatches its path if it is a filesystem directory
    inline void entity_model::m_deactivate(int key)
    {
        if (m_tree.contains(key))
        {
            const auto& qd = m_node_for_key(key).descriptor;
            m_dataman->cancel_query(qd);
            m_dataman->unwatch_path(qd.text);
            m_remove_delayed_info_fetch(key);
            m_delayed_sort_timer.stop();
        }
    }

    inline void entity_model::m_schedule_delayed_info_fetch(int key, int start_index)
    {
        if (m_tree.contains(key) && start_index < m_tree[key].size())
        {
            m_delayed_info_fetches.emplace_back(key, start_index);
            m_delayed_info_fetch_timer.start(0, this);
        }
    }

    inline void entity_model::m_remove_delayed_info_fetch(int key, int start_index)
    {
        if (key == 0 && start_index == -1)
        { m_delayed_info_fetches.pop_front(); }
        else
        {
            auto new_end = std::remove_if(m_delayed_info_fetches.begin(), m_delayed_info_fetches.end(),
                [&](const std::tuple<int, int>& t) -> bool
                {
                    if (start_index == -1)
                    { return std::get<0>(t) == key; }

                    const auto [k, i] = t;
                    return k == key && i == start_index;
                }
            );
            m_delayed_info_fetches.erase(new_end, m_delayed_info_fetches.end());
        }
        if (m_delayed_info_fetches.empty())
        { m_delayed_info_fetch_timer.stop(); }
    }

    inline void entity_model::m_schedule_delayed_sort(const int key, int delay)
    {
        if (m_tree.contains(key))
        {
            m_delayed_sorts.push_back(key);
            m_delayed_sort_timer.start(delay, this);
        }
    }

    inline void entity_model::m_remove_delayed_sort(const int key)
    {
        if (m_delayed_sorts.size())
        {
            if (key)
            {
                auto new_end = std::remove(m_delayed_sorts.begin(), m_delayed_sorts.end(), key);
                m_delayed_sorts.erase(new_end, m_delayed_sorts.end());
            }
            else
            { m_delayed_sorts.pop_front(); }
        }

        if (m_delayed_sorts.empty())
        { m_delayed_sort_timer.stop(); }
    }

    inline void entity_model::m_interrupt_delayed_sort()
    {
        m_delayed_sort_timer.stop();
    }

    inline void entity_model::m_schedule_delayed_query(const QString& query_string)
    {
        m_pending_queries.push_back(query_string);
        m_pending_query_timer.start(0, this);
    }

    inline void entity_model::m_remove_delayed_query(const QString& query_string)
    {
        if (query_string.isEmpty())
        { m_pending_queries.pop_front(); }
        else
        {
            auto new_end = std::remove(m_pending_queries.begin(), m_pending_queries.end(), query_string);
            m_pending_queries.erase(new_end, m_pending_queries.end());
        }

        if (m_pending_queries.empty())
        { m_pending_query_timer.stop(); }
    }

    inline void entity_model::m_do_invalidate(entity_tree_node& node)
    {
        using enum entity_tree_node_state;
        using enum query_descriptor::query_mode;
        const QString& ft = node.descriptor.full_text();
        
        qDebug() << "entity_model: begin invalidate: " << ft;
        
        m_dataman->cancel_query(ft, node.descriptor.mode);
        m_dataman->cancel_query(ft, Read_Extended);
        m_dataman->cancel_query(ft, Sort);
        node.state = Invalidated;
    }

    inline void entity_model::m_create_node(const QString& query_text, int parent_key, int index)
    {
        assert(query_text.size());
        std::optional<entity_attribute_type> sort_key = std::nullopt;
        std::optional<Qt::SortOrder> sort_order = std::nullopt;
        if (m_sort_column && m_sort_order)
        {
            sort_key = std::get<0>(m_field_names[*m_sort_column]);
            sort_order = m_sort_order;
        }
        m_dataman->query(query_text, m_tree, index, parent_key, sort_key, sort_order);
    }

    inline void entity_model::m_create_node(const QString& query_text, const QList<QUrl>& urls, int parent_key, int index)
    {
        assert(query_text.size() && query_text.startsWith("concept://"));
        m_dataman->query(query_text, m_tree, urls, index, parent_key);
    }

    inline void entity_model::m_remove_node(int key)
    {
        qDebug() << "entity_model: remove node and all child nodes: " << m_id_for_key(key);

        m_tree.traverse(key, [this](const entity_tree_node& node, int depth) -> bool
        {
            Q_UNUSED(depth);
            qDebug() << "entity_model: remove node: " << m_id_for_key(node.key);

            m_dataman->unwatch_path(node.descriptor.text);
            m_dataman->cancel_query(node.descriptor);
            m_tree[node.key].state = entity_tree_node_state::Pending_Removal;

            beginRemoveRows(m_index_for_querytext(node.descriptor.full_text()), 0, static_cast<int>(node.items.size()) - 1);

            if (m_root && *m_root == node.key)
            { m_root = std::nullopt; }

            m_tree.erase(node.key);

            endRemoveRows();

            return false;
        });
    }

    inline void entity_model::m_remove_item(int parent_key, int index)
    {        
        if (m_tree.contains(parent_key) && std::cmp_less(index, m_tree[parent_key].size()))
        {
            entity_tree_node& node = m_tree[parent_key];            
            std::unordered_map<QString, QModelIndex> old = m_indexes(node);
            QList<QModelIndex> old_indexes;
            QList<QModelIndex> new_indexes;
            old_indexes.reserve(node.size());
            new_indexes.reserve(node.size());
            old.erase(m_tree[parent_key].positions[index]);
            
            beginRemoveRows(m_index_for_querytext(node.descriptor.full_text()), index, index);
            m_tree.erase(parent_key, index);

            int i = 0;
            for (const auto& name : node)
            {
                const auto& item = node[name];
                if (m_tree.contains(item.key()))
                {
                    m_tree[item.key()].index = i;
                }
                old_indexes << old[name];
                new_indexes << m_index_for_item(item, i);
                ++i;
            }
            changePersistentIndexList(old_indexes, new_indexes);
            endRemoveRows();
        }
    }

    // calls m_remove_item and m_remove_node if item is also a node
    inline void entity_model::m_purge_item(int parent_key, int item_key, int item_index)
    {
        if (m_tree.contains(item_key))
        { m_remove_node(item_key); }

        m_remove_item(parent_key, item_index);
    }

    inline std::unordered_map<QString, QModelIndex> entity_model::m_indexes(const entity_tree_node& node) const
    {
        std::unordered_map<QString, QModelIndex> indexes;
        indexes.reserve(node.size());
        for (int i = 0; i < node.size(); ++i)
        {
            indexes.emplace(node.positions[i], m_index_for_item(node[i], i));
        }
        return indexes;
    }

    inline QModelIndex entity_model::m_index_for_item(const reflexive_entity& item, int pos) const
    {
        assert(item.parent_key());
        return createIndex(pos, 0, item.parent_key());
    }

    // assumes text is a query_string that represents a node
    inline QModelIndex entity_model::m_index_for_querytext(const QString& text) const
    {
        const entity_tree_node& node = m_node_for_key(m_tree.key_for_querytext(text));

        // node.parent is not null if we have an index value
        return createIndex(node.index, 0, node.index == -1 ? node.key : node.parent);
    }

    inline const entity_tree_node& entity_model::m_node_for_key(int key) const
    {
        return m_tree[key];
    }

    // as the function name would suggest, bad things will happen if this is called with an invalid or partially-valid index
    inline const entity_tree_node& entity_model::m_node_for_valid_index(const QModelIndex& index) const
    {
        return m_tree[static_cast<int>(index.internalId())];
    }

    // returns the containing node for index or the root node if index is invalid
    // do not call this function if there is no root set
    inline const entity_tree_node& entity_model::m_node_for_index(const QModelIndex& index) const
    {
        if (const int key = static_cast<int>(index.internalId()); key && m_valid_index(index))
        {
            return m_tree[key];
        }
        return m_tree[*m_root];
    }

    inline QString entity_model::m_id_for_key(int key) const
    {
        return m_tree[key].descriptor.text;
    }

    // TODO
    inline QString entity_model::m_absolute_path_for_file_entity(const QModelIndex& index) const
    {
        return m_id_for_index(index);
    }

    // if the index is valid or partially-valid: returns a valid item id or the root's id (query string) respectively
    inline QString entity_model::m_id_for_index(const QModelIndex& index) const
    {
        using enum query_descriptor::query_type;
        using enum entity_attribute_type;
        const entity_tree_node& node = m_node_for_index(index);
        const int i = index.row();

        if (std::cmp_less(i, node.size()) && i >= 0)
        {
            if (node[i].type() == sz::entity_type::File)
            {
                QString id = node[i].attribute<QFileInfo>(File_Info).absoluteFilePath();
                if (!QFile::exists(id))
                {
                    // the assumption is that every file entity in the model exists on the filesystem
                    auto* this_ = const_cast<entity_model*>(this);
                    this_->m_refresh_entity(this_->m_tree[node.key][i]);
                    id = node[i].attribute<QFileInfo>(File_Info).absoluteFilePath();
                }
                return id;
            }
            return node[i].attribute<QString>(Name);
        }
        else if (i == -1 && index.column() == 0) // either root or a former root
        {
            return node.descriptor.text;
        }
        
        return m_root ? m_id_for_key(*m_root) : QString();
    }

    // does not check if index is invalid
    inline const reflexive_entity& entity_model::m_item_for_index(const QModelIndex& index) const
    {
        return m_node_for_valid_index(index)[index.row()];
    }

    inline QModelIndex entity_model::m_reset_root_query(const QString& text)
    {
        QModelIndex index;
        if (m_tree.contains(text))
        {
            if (m_root)
            { m_deactivate(*m_root); }

            m_root = m_tree[text].key;
            m_dataman->watch_path(text);

            index = m_index_for_querytext(text);
        }
        return index;
    }

    inline bool entity_model::m_valid_index(const QModelIndex& index) const
    {
        const int r = index.row();
        const int c = index.column();
        const int key = static_cast<int>(index.internalId());

        if (m_tree.contains(key))
        {
            const auto& node = m_node_for_key(key);
            return (r >= -1 && std::cmp_less(r, node.items.size())) && (c >= 0 && std::cmp_less(c, m_field_names.size()));
        }

        return false;
    }

    // does no checks
    inline const QPixmap& entity_model::m_thumbnail_for_index(const QModelIndex& index) const
    {
        const int key = m_key_for_index(index);
        return m_thumbnails.at(key).at(m_tree[key].positions[index.row()]);
    }

    inline void entity_model::m_async_sort(int key, int column, Qt::SortOrder order)
    {
        if (column < 0 || column >= columnCount() || !m_tree.contains(key))
        { return; }

        m_async_sort(key, std::get<0>(m_field_names[column]), order);
    }

    inline void entity_model::m_async_sort(int key, entity_attribute_type sort_key, Qt::SortOrder sort_order)
    {
        auto& node = m_tree[key];
        node.descriptor.sort_key = sort_key;
        node.descriptor.sort_order = sort_order;
        m_dataman->sort(node, m_tree.keys());
    }

    inline void entity_model::m_sort(int key, int column, Qt::SortOrder order)
    {
        if (column < 0 || column >= columnCount() || !m_tree.contains(key))
        { return; }

        qDebug() << "entity_model: begin sort for node: " << m_tree[key].descriptor.full_text();

        emit sort_starting();
        auto old = m_indexes(m_tree[key]);
        QList<QModelIndex> old_indexes;
        QList<QModelIndex> new_indexes;
        auto& node = m_tree[key];
        node.descriptor.sort_key = std::get<0>(m_field_names[column]);
        node.descriptor.sort_order = order;
        rin::sort_entities(node);
        for (int i = 0; std::cmp_less(i, node.size()); ++i)
        {
            const auto& item = node[i];
            if (m_tree.contains(item.key()))
            {
                m_tree[item.key()].index = i;
            }
            old_indexes << old[m_tree[key].positions[i]];
            new_indexes << m_index_for_item(item, i);
        }
        changePersistentIndexList(old_indexes, new_indexes);
        emit sort_finished();
    }
}
