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

namespace rin
{
    static constexpr size_t s_default_batchsize = 10000;
    static constexpr size_t s_max_in_memory = 100000;

    entity_model::entity_model(const std::filesystem::path& database_path, QObject* parent) :
        QAbstractItemModel(parent), 
        m_dataman(std::make_unique<entity_data_manager>(database_path, this)),
        m_thumbman(std::make_unique<thumbnail_manager>(database_path)),
        m_database_path(database_path),
        m_tree(),
        m_thumbnails(), m_items_pending_update(),
        m_pending_queries(), m_delayed_sorts(), m_pending_query_timer(), m_delayed_sort_timer(),
        m_root(std::nullopt), m_sort_column(std::nullopt), m_sort_order(std::nullopt),
        m_thumbnail_target_size(sz::thumbnail_target_size::Normal_128px),
        m_batchsize(100),
        m_readonly(true)
    {
        sz::entity::set_non_native_default<QMimeType>(Mime_Type);
        sz::entity::set_adaptor<sz::string>([](const sz::entity_attribute& v) -> sz::string
        {
            if (v.hash() == sz::hash_for<QString>()) {
                return v.get<QString>().toStdString();
            }
            return sz::string();
        });

        auto* dataman = m_dataman.get();
        connect(dataman, &entity_data_manager::data_available, this, &entity_model::insert);
        m_dataman->set_watcher_enabled(false);
        
        auto* thumbman = m_thumbman.get();
        connect(thumbman, &thumbnail_manager::thumbnail_available, this, &entity_model::set_thumbnail);
        
        set_display_attributes({
            Name, Size, Modified, File_Type
        });
        set_display_attribute_string(File_Type, "Type");
    }

    entity_model::~entity_model()
    {
        // if/when manager threads inherit a common base, we should constrain the template here
        auto quit_thread = []<typename T>(std::unique_ptr<T>& thread) -> void
        {
            thread->request_abort();
            if (!thread->wait(1000))
            {
                auto* t = thread.release();
                t->deleteLater();
            }
        };

        quit_thread(m_dataman);
        quit_thread(m_thumbman);
    }

    void entity_model::set_batchsize(usize size)
    {
        m_dataman->set_batchsize(static_cast<int>(size));
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
        clear_thumbnails();
        m_thumbman->set_database(path);
        m_database_path = path;
    }

    void entity_model::set_display_attributes(const std::vector<entity_attribute_type>& attrs)
    {
        m_attribute_section_positions = attrs;
        for (auto k : attrs)
        {
            m_attribute_names[k] = QString::fromStdString(sz::utility::to_string(k));
        }
    }

    void entity_model::set_display_attribute_string(entity_attribute_type attr, QString str)
    {
        m_attribute_names[attr] = str;
    }

    void entity_model::set_watched(const QModelIndex& index, bool watched)
    {
        if (const QString uri = m_uri_for_index(index); m_tree.contains(uri)) {
            m_dataman->set_watched(uri, watched);
        }
    }

    // root queries with no preexisting item entry have a -1 row value in the returned index
    QModelIndex entity_model::query(QString text, bool replace_root)
    {
        using enum sz::entity_type;

        if (text.isEmpty()) {
            return QModelIndex();
        }
        if (replace_root && m_root) {
            m_deactivate(*m_root);
        }
        const std::string text_ = text.toStdString();
        auto [id_, uri_type] = sz::id_from_uri(text_);
        const QString id = QString::fromUtf8(id_.data(), id_.size());
        const bool file_exists = QFileInfo::exists(id);

        if (uri_type == Invalid)
        {
            uri_type = file_exists ? File : Tag; // concept queries must always explicitly use the concept uri
            text = QString::fromStdString(sz::entity_uri(text_, uri_type));
        }
        if (m_tree.contains(text)) {
            invalidate(m_tree.key_for_uri(text));
        }
        else
        {
            data_node_key pk = 0; // 0 is never a valid key and is used to signify that a node does not have a parent
            int i = -1;
            if (uri_type == File && file_exists)
            {
                const QFileInfo info(id);
                const QString parent_uri = QString::fromStdString(sz::entity_uri(info.absoluteDir().canonicalPath().toStdString(), File));
                if (m_tree.contains(parent_uri))
                {
                    pk = m_tree.key_for_uri(parent_uri);
                    i = m_tree.index_of(pk, info.fileName());
                }
            }
            // TODO: parent/child related index/concept queries
            m_create_and_enqueue_node(text, pk, i);
        }
        if (replace_root || (!m_root)) {
            m_root = m_tree.key_for_uri(text);
        }
        return m_index_for_uri(text);
    }

    void entity_model::sort(const QModelIndex& parent, int column, Qt::SortOrder order)
    {
        using namespace query_operator;
        
        if (m_valid_index(parent) && column > -1 && std::cmp_less(column, m_attribute_section_positions.size()))
        {
            data_node_key k = 0;
            
            if (parent.row() > -1)
            {
                const auto& e = m_entity_for_index(parent);
                
                if (const auto ek = e.key(); m_tree.contains(ek)) {
                    k = ek;
                }
            }
            else {
                k = m_key_for_index(parent);
            }
            if (m_tree.contains(k)) // 0 is never a valid key
            {
                m_tree.apply_keymap(k);

                auto n = m_tree.data_node_for_key(k);

                n->set_sort_info(m_attribute_section_positions[column], order);
                m_dataman->enqueue(n, nullptr, Sort, enqueue_position::Back);
            }
        }
    }

    void entity_model::tag(const QModelIndex& index, const std::vector<reflexive_entity>& tag_entities)
    {
        // if (index.isValid())
        // {
        //     auto& e = m_item_for_index(index);
            
        //     QString entity_id;
        //     if (e.has_attribute(Id))
        //     { entity_id = e.attribute<QString>(Id); }
        //     else
        //     { entity_id = m_id_for_index(index); }

        //     std::vector<std::string> tags;
        //     for (const auto& tag_entity : tag_entities)
        //     {
        //         // tag name is equivalent to tag entity id
        //         tags.push_back(tag_entity.attribute<QString>(Name).toStdString());
        //     }

        //     sz::metadata::parameters args{
        //         .tags = tags,
        //         .sidecar_keys{},
        //         .sidecar_split_character{},
        //         .sidecar_alternate_root_path{},
        //         .sidecar_type{},
        //         .use_sidecars = false,
        //         .check_tags = false
        //     };
        //     m_in_progress_db_writes.emplace(entity_id, m_dataman->tag(entity_id, args));
        // }
    }

    void entity_model::tag(const QModelIndex& index, const sz::metadata::parameters& args)
    {
        // if (index.isValid())
        // {
        //     const QString entity_id = m_id_for_index(index);
        //     m_in_progress_db_writes.emplace(entity_id, m_dataman->tag(entity_id, args));
        // }
    }

    void entity_model::tag(const QString& entity_id, const sz::metadata::parameters& args)
    {
        // if (entity_id.size())
        // { m_in_progress_db_writes.emplace(entity_id, m_dataman->tag(entity_id, args)); }
    }

    void entity_model::append_data(const QModelIndex& parent, const std::vector<reflexive_entity>& data)
    {
        // if (m_is_concept(parent))
        // {
        //     auto qd = m_tree[m_id_for_index(parent)].descriptor;
        //     insert(qd, data);
        // }
    }

    bool entity_model::remove(const QModelIndex& index, bool recycle)
    {
        if (!m_valid_index(index))
        { return false; }

        // if (const QString id = m_id_for_index(index); id.size())
        // {
        //     const int key = m_key_for_index(index);
        //     const QString parent_id = m_tree[key].descriptor.text;
        //     const auto& item = m_item_for_index(index);

        //     m_dataman->unwatch_path(parent_id);
        //     m_dataman->fetch(parent_id, {entity_model_url{
        //         .src = QUrl::fromLocalFile(id),
        //         .target_name = item.attribute<QString>(Name),
        //         .index = index.row()
        //     }}, recycle ? fetch_action::Recycle : fetch_action::Remove, false);
        //     return true;
        // }
        return false;
    }

    void entity_model::clear(const QModelIndex& parent)
    {
        if (m_valid_index(parent))
        {
            if (const auto uri = m_uri_for_index(parent); m_tree.contains(uri)) {
                // clear(m_tree[query_text].key(), false);
            }
        }
    }

    void entity_model::clear_thumbnails()
    {
        // dataChanged will be emitted when the model receives a new thumbnail from thumbman
        m_thumbman->clear();
        m_thumbnails.clear();
    }

    // assumes all indexes to be valid
    void entity_model::request_thumbnails(const QList<QModelIndex>& indexes, sz::thumbnail_target_size size)
    {   
        std::vector<thumbnail_descriptor> batch;
        for (const auto& index : indexes)
        {
            if (const auto& e = m_entity_for_index(index); e.has_attribute(Sizehint))
            {
                thumbnail_descriptor d(m_uri_for_index(index));
                d.entity_name = e.attribute<QString>(Name);
                d.original_size = e.attribute<QSize>(Sizehint);
                d.target_size = size;
                d.entity_type = e.type();
                d.node_key = m_key_for_index(index);
                batch.emplace_back(std::move(d));
            }
        }
        m_thumbman->submit_batch(batch);
    }

    // returns true if index refers to a node or entity
    bool entity_model::valid_index(const QModelIndex& index) const
    {
        return m_valid_index(index);
    }

    bool entity_model::extended_information_loaded(const QModelIndex& index) const
    {
        using enum sz::entity_type;
        
        if (index.isValid())
        {
            const auto& e = m_entity_for_index(index);
            if (e.type() == File) {
                return e.has_attribute(Mime_Type);
            }
            return e.has_attribute(Icon);
        }
        return false;
    }

    QPixmap entity_model::thumbnail(const reflexive_entity& e, sz::thumbnail_target_size size) const
    {
        const auto name = e.attribute<QString>(Name);
        const data_node_key key = e.parent_key();
        
        if (m_has_thumbnail_loaded(name, key, size)) {
            return m_thumbnail_for_entity(name, key, size);
        }
        return QPixmap();
    }

    QPixmap entity_model::thumbnail(const reflexive_entity& e) const
    {
        return thumbnail(e, m_thumbnail_target_size);
    }

    QPixmap entity_model::thumbnail(const QModelIndex& index) const
    {
        if (index.isValid()) {
            return thumbnail(m_entity_for_index(index));
        }
        return QPixmap();
    }

    bool entity_model::has_thumbnail(const QModelIndex& index) const
    {
        if (m_valid_index(index) && index.row() != -1 && index.column() == 0) {
            return m_entity_for_index(index).has_attribute(Sizehint);
        }
        return false;
    }

    bool entity_model::has_thumbnail_loaded(const QModelIndex& index, sz::thumbnail_target_size size) const
    {
        if (m_valid_index(index) && index.row() != -1 && index.column() == 0)
        {
            const data_node_key key = m_key_for_index(index);
            const auto name = m_node_for_key(key)[index.row()].attribute<QString>(Name);
            return m_has_thumbnail_loaded(name, key, size);
        }
        return false;
    }

    bool entity_model::has_thumbnail_loaded(const QModelIndex& index) const
    {
        return has_thumbnail_loaded(index, m_thumbnail_target_size);
    }

    QModelIndex entity_model::root_index() const
    {
        if (m_root) {
            return m_index_for_uri(m_uri_for_key(*m_root));
        }
        return QModelIndex();
    }

    QModelIndex entity_model::index(const QString& uri)
    {
        if (m_tree.contains(uri)) {
            return m_index_for_uri(uri);
        }
        return QModelIndex();
    }

    QString entity_model::root_uri() const
    {
        if (m_root) {
            return m_node_for_key(*m_root).const_descriptor().uri();
        }
        return QString();
    }

    QString entity_model::id_for_index(const QModelIndex& index) const
    {
        if (index.isValid())
        {
            const auto& e = m_entity_for_index(index);
            return e.attribute<QString>(Id);
        }
        else if (m_valid_index(index)) {
            return m_node_for_index(index).const_descriptor().text();
        }
        return QString();
    }

    const reflexive_entity& entity_model::at(const QModelIndex& index)
    {
        if (index.isValid())
        {
            reflexive_entity& e = m_entity_for_index(index);
            m_refresh_entity(e);
            return std::as_const(e);
        }
        throw sz::exception(sz::result_code::Invalid_Argument, "entity_model::at: index was invalid");
    }

    bool entity_model::entity_exists(const std::string& entity_id) const
    {
        sz::entity_database db(m_database_path, true);
        return db.entity_exists(entity_id);
    }

    reflexive_entity entity_model::entity_from_id(const std::string& id) const
    {
        using enum sz::entity_type;
        
        static const QMimeDatabase mimedb;
        sz::entity_database db(m_database_path, true);
        auto e = reflexive_entity::from_uri_or_id(QString::fromStdString(id), &db);
        e.fill_extended_attributes(db, mimedb, true);
        return e;
    }

    bool entity_model::setData(const QModelIndex& index, const QVariant& value, int role)
    {
        // TODO: depending on the entity, we would need to query a filesystem write or a database write
        using enum entity_tree_node_state;
        if (value.isNull() || !m_valid_index(index))
        { return false; }

        // const auto& item = m_item_for_index(index);
        // const QString query_text = m_tree[item.parent_key()].const_descriptor().text();
        
        // if (role == Qt::EditRole && item.type() == sz::entity_type::File)
        // {
        //     m_tree[item.parent_key()].state() = Updating;
        //     const QString old_name = item.attribute<QString>(Name);
        //     const QString new_name = value.toString();
        //     const QString parent_dir_path = item.attribute<QFileInfo>(File_Info).absoluteDir().canonicalPath() + QDir::separator();
        //     const QUrl url = QUrl::fromLocalFile(parent_dir_path + old_name);
        //     const QString new_path = parent_dir_path + new_name;
            
        //     qDebug() << "entity_model::setData: set value: " << value << ", to index: " << index << ", url used: " << url;
            
        //     const auto action = fetch_parameters::action::Move;

        //     if (m_tree.contains(item.key()))
        //     {
        //         m_tree.traverse(item.parent_key(), [this](const entity_tree_node& node, int depth) -> bool
        //         {
        //             Q_UNUSED(depth);
        //             m_dataman->unwatch_path(node.const_descriptor().text());
        //             return false;
        //         });
        //     }
        //     else
        //     {
        //         m_dataman->unwatch_path(query_text);
        //     }
        //     m_dataman->fetch(
        //         query_text,
        //         {entity_model_url{
        //             .src = url,
        //             .target_name = new_name,
        //             .index = index.row()
        //         }},
        //         action, true
        //     );
        //     m_items_pending_update[m_tree[item.parent_key()].descriptor.full_text()].emplace(new_name, item_temp_data{
        //         .old_name = old_name,
        //         .index = index.row(),
        //         .action = action
        //     });
            
        //     return true; // update() will emit dataChanged if the rename was successful
        // }
        // // TODO: other entity types
        return false;
    }

    bool entity_model::setHeaderData(int section, Qt::Orientation orientation, const QVariant& value, int role)
    {
        if (
            std::cmp_greater_equal(section, m_attribute_section_positions.size()) ||
            (section < 0) ||
            (orientation == Qt::Orientation::Vertical) ||
            (role != Qt::DisplayRole && role != Qt::EditRole) ||
            (value.userType() != QMetaType::QString)
        )
        { return false; }

        set_display_attribute_string(m_attribute_section_positions[section], value.toString());
        emit headerDataChanged(orientation, section, section);
        return true;
    }

    // TODO: handle drops that occur on indexed queries
    // TODO: handle cases where an item is indexed
    // FIXME: drops that occur for items that don't yet have a node don't have their size attribute updated
    bool entity_model::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent)
    {
        using enum query_source;
        Q_UNUSED(row);
        Q_UNUSED(column);
        qDebug() << "entity_model::dropMimeData: parent: " << parent << ", row: " << row << ", col: " << column << ", action: " << action;
        
        bool ok = false;
        
        // if (m_readonly)
        // { return ok; }

        // if (flags(parent) & Qt::ItemIsDropEnabled)
        // {
        //     ok = true;
        //     const QString id = m_id_for_index(parent);
        //     m_dataman->unwatch_path(id); // otherwise the path will be invalidated when the fetch starts

        //     std::vector<entity_model_url> urls;
        //     for (const auto& qurl : data->urls())
        //     {
        //         urls.emplace_back(entity_model_url{
        //             .src = qurl,
        //             .target_name = QFileInfo(qurl.toLocalFile()).fileName(),
        //             .index = -1 // non-update fetches will trigger an insert()
        //         });
        //     }
        //     m_dataman->fetch(id, std::move(urls), fetch_action{action});
        // }
        // else
        // {
        //     qDebug() << "entity_model::dropMimeData: not an item that accepts drops";
        //     ok = false;
        // }
        
        return ok;
    }

    // this function has no effect if given a fully invalid index
    // TODO (entity_data_manager): concurrency for query jobs
    // TODO: this function is only relevant if batchsize < node.size but currently (2026/11/17)
    //       dataman will continue working on queries until its queue is empty.
    //       we should implement a pause functionality for dataman if we want it to stop
    //       after each batch
    void entity_model::fetchMore(const QModelIndex& parent)
    {
        if (m_valid_index(parent))
        {
            if (parent.row() > -1)
            {
                const auto& e = m_entity_for_index(parent);
                const auto uri = QString::fromStdString(sz::entity_uri(m_id_for_index(parent).toStdString(), e.type()));
                m_create_and_enqueue_node(uri, e.parent_key(), parent.row());
            }
            // else
            // {
            //     // it already exists as a node
            //     // we should assume canFetchMore(parent) returns true
            //     // ...just call dataman->enqueue again?
            // }
        }
    }

    // this function does not actually sort the model and is equivalent to set_sort_preference(column, order)
    void entity_model::sort(int column, Qt::SortOrder order)
    {
        set_sort_preference(column, order);
    }

    QModelIndex entity_model::index(int row, int column, const QModelIndex& parent) const
    {
        if (row >= rowCount(parent) || column >= columnCount(parent) || column < 0) // -1 row, 0 col refers to a node
        { return QModelIndex(); }

        const auto& n = m_node_for_index(parent);
        if (const data_node_key k = parent.row() == -1 ? n.key() : m_tree[n.key()][parent.row()].key(); m_tree.contains(k))
        { return createIndex(row, column, k); }
        
        return QModelIndex();
    }
    
    QModelIndex entity_model::parent(const QModelIndex& child) const
    {
        if (m_valid_index(child))
        {
            const auto& node = m_node_for_valid_index(child);
            return createIndex(node.index(), 0, m_tree.contains(node.parent_key()) ? node.parent_key() : node.key());
        }
        return QModelIndex();
    }

    int entity_model::rowCount(const QModelIndex& parent) const
    {
        int count = 0;
        if (m_root && (!m_valid_index(parent)))
        {
            count = m_node_for_key(*m_root).size();
        }
        else if (const QString uri = m_uri_for_index(parent); m_tree.contains(uri))
        {
            count = m_node_for_uri(uri).size();
        }
        return count;
    }

    int entity_model::columnCount(const QModelIndex& parent) const
    {
        Q_UNUSED(parent);
        return static_cast<int>(m_attribute_section_positions.size());
    }

    bool entity_model::hasChildren(const QModelIndex& parent) const
    {
        if (m_root && (!m_valid_index(parent) || parent.row() == -1))
        {
            return m_node_for_key(*m_root).const_descriptor().size() > 0;
        }
        return m_entity_for_index(parent).has_children();
    }

    // this function cannot be used to return information on nodes
    QVariant entity_model::data(const QModelIndex& index, int role) const
    {
        if (!index.isValid())
        { return QVariant(); }

        const auto& e = m_entity_for_index(index);
        const int col = index.column();
        
        switch (role)
        {
        case Qt::DisplayRole:
        {
            // TODO: handling non-string attributes (need to_string() impls for all attributes)
            if (std::cmp_less(col, m_attribute_section_positions.size()))
            {
                if (const entity_attribute_type attr = m_attribute_section_positions[col]; e.has_attribute(attr))
                {
                    // TODO: stronger run-time validation of attribute type
                    return e.attribute<QString>(attr);
                }
            }
            break;
        }

        case Qt::EditRole:
        { return e.attribute<QString>(Name); }

        case Qt::DecorationRole:
        {
            const QString& name = e.attribute<QString>(Name);
            const int key = e.parent_key();
            if (m_has_thumbnail_loaded(name, key))
            {
                return QIcon(m_thumbnail_for_entity(name, key));
            }
            else if (e.has_attribute(Sizehint))
            {
                auto* this_ = const_cast<entity_model*>(this);
                this_->request_thumbnails({index}, m_thumbnail_target_size);
            }

            if (e.has_attribute(Icon))
            { return e.attribute<QIcon>(Icon); }

            if (e.has_attribute(Mime_Type))
            {
                if (const auto& mime = e.attribute<QMimeType>(); m_icon_cache.contains(mime))
                { return m_icon_cache[mime]; }
            }

            if (e.type() == sz::entity_type::File)
            { return m_icon_provider.icon(e.attribute<QFileInfo>()); }

            break;
        }

        case Qt::SizeHintRole:
        {
            if (e.has_attribute(Sizehint))
            { return e.attribute<QSize>(Sizehint); }
            
            return QSize();
        }
        
        default:
        { break; }
        }

        return QVariant();
    }

    QVariant entity_model::headerData(int section, Qt::Orientation orientation, int role) const
    {
        if (
            std::cmp_greater_equal(section, m_attribute_section_positions.size()) ||
            (section < 0) ||
            (orientation == Qt::Orientation::Vertical) ||
            (role != Qt::DisplayRole)
        )
        { return QVariant(); }

        return m_attribute_names.at(m_attribute_section_positions[section]);
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
        using enum sz::entity_type;
        QList<QUrl> urls;
        for (const auto& index : indexes)
        {
            const auto& e = m_entity_for_index(index);
            if ((e.type() == File) && std::cmp_less(index.column(), m_attribute_section_positions.size()) && m_attribute_section_positions[index.column()] == Name)
            {
                urls << QUrl::fromLocalFile(m_absolute_path_for_entity(index));
            }
        }
        QMimeData* data = new QMimeData();
        data->setUrls(urls);
        return data;
    }

    bool entity_model::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) const
    {
        using enum query_source;
        Q_UNUSED(row);
        Q_UNUSED(column);

        if (m_node_for_index(parent).const_descriptor().source() == Index)
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

    Qt::DropActions entity_model::supportedDropActions() const
    {
        return Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
    }

    // TODO: this might have to change if the root query represents a concept entity
    Qt::DropActions entity_model::supportedDragActions() const
    {
        return supportedDropActions();
    }

    bool entity_model::canFetchMore(const QModelIndex& parent) const
    {
        if (m_valid_index(parent))
        {
            if (parent.row() > -1)
            {
                const auto& e = m_entity_for_index(parent);

                if (const auto ek = e.key(); m_tree.contains(ek))
                {
                    const auto& n = m_node_for_key(ek);

                    return std::cmp_less(n.size(), n.const_descriptor().size());
                }
                return e.has_children();
            }
            const auto& n = m_node_for_valid_index(parent);

            return std::cmp_less(n.size(), n.const_descriptor().size());
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

        if (const reflexive_entity& e = m_node_for_valid_index(index)[index.row()]; e.type() == File)
        {
            flags |= Qt::ItemIsDragEnabled;
            const QFileInfo& info = e.attribute<QFileInfo>(File_Info);

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

    void entity_model::set_thumbnail_target_size(sz::thumbnail_target_size size)
    {
        m_thumbnail_target_size = size;
    }

    void entity_model::set_sort_preference(int column, Qt::SortOrder order)
    {
        qDebug() << "entity_model::set_sort_preference(): " << column << ", " << order;
        m_sort_column = column;
        m_sort_order = order;
    }

    void entity_model::insert(data_node_update update)
    {
        using namespace query_operator;

        if (const data_node_key k = update.node->key(); m_tree.contains(k) && update.start <= update.end)
        {
            bool node_assigned = false;
            bool node_sorted = false;
            bool removals_evaluated = false;

            auto assign_node = [&]() -> void
            {
                if (!node_assigned) {
                    m_tree.assign_node(k, update.node);
                }
                node_assigned = true;
            };
            auto sort_node = [&]() -> void
            {
                if (!node_sorted)
                {
                    emit sort_starting();
                    assign_node();

                    if (update.indexes_changed_from_sort) {
                        m_update_indexes(*update.indexes_changed_from_sort);
                    }
                    emit sort_finished();
                }
                node_sorted = true;
            };
            auto evaluate_removals = [&](const data_node& prev) -> void
            {
                // TODO: need to update keymap before getting changes from removal
                if (!removals_evaluated && update.indexes_removed)
                {
                    // do not take references from the tree here. node removals might cause a change to underlying memory
                    const auto& n = *prev;
                    for (const auto& [name, i] : *update.indexes_removed)
                    {
                        if (const data_node_key ek = n[name].key(); m_tree.contains(ek)) {
                            m_remove_node(ek);
                        }
                    }
                    assign_node();
                    m_update_indexes(*update.indexes_changed_from_remove);
                }
                removals_evaluated = true;
            };
            
            if (const data_node prev = m_tree.data_node_for_key(k); update.flags & Read)
            {
                const QModelIndex parent = m_index_for_uri(update.node->const_descriptor().uri());

                beginInsertRows(parent, update.start, update.end);
                assign_node();
                endInsertRows();

                if (update.flags & Sort) {
                    sort_node();
                }
                evaluate_removals(prev);
            }
            else if (update.flags & Sort) {
                sort_node();
            }
            else if (update.indexes_removed) {
                evaluate_removals(prev);
            }
        }
    }

    // unwatching the path doesn't seem to be ideal here, as the path could change inbetween invalidates.
    // every time the watcher notifies for update, we re-invalidate the node to ensure we always have the most up-to-date version of source in dataman
    void entity_model::invalidate(data_node_key k)
    {
        using namespace query_operator;

        if (m_tree.contains(k))
        {
            m_dataman->dequeue(k);
            data_node n = m_tree.data_node_for_key(k);
            n->set_state(entity_tree_node_state::Invalidated);
            m_dataman->enqueue(n, nullptr, Read | Read_Extended, enqueue_position::Front);
        }
    }

    void entity_model::set_thumbnail(const thumbnail_descriptor& d)
    {
        const data_node_key key = d.node_key;
        m_thumbnails[key][d.entity_name][d.target_size] = d.thumbnail;

        // rather than call tree.index_of() for every individual entity, it's faster to just tell views that the entire node has changed in some way
        // as of 2026/10/06, entity_view always does a full repaint regardless of which index was changed
        const auto& n = m_node_for_key(key);
        const int last_idx = n.size() - 1;
        const QModelIndex first = m_index_for_entity(n[0], 0);
        const QModelIndex last = m_index_for_entity(n[last_idx], last_idx);
        
        emit thumbnail_available(n[d.entity_name], d.target_size);
        emit dataChanged(first, last, {Qt::DecorationRole});
    }

    // TODO: delayed queries
    void entity_model::timerEvent(QTimerEvent* e)
    {
        QObject::timerEvent(e);
    }

    sz::result<entity_tree_node_state> entity_model::node_state(const QModelIndex& parent) const
    {
        const auto& node = m_node_for_index(parent);
        const int index = parent.row();
        const int key = index > -1 && std::cmp_less(index, node.size()) ? node[index].key() : 0;
        
        if (m_tree.contains(key)) {
            return m_tree[key].state();
        }
        else if (m_root) {
            qDebug() << "entity_model: return state for root: " << m_tree[*m_root].const_descriptor().uri() << ", index was: " << parent;
        }
        return sz::common_error(sz::result_code::Invalid_Argument);
    }

    // used by dataman to create indexes on behalf of the model
    QModelIndex entity_model::create_index(const reflexive_entity& e, int idx) const
    {
        return m_index_for_entity(e, idx);
    }

    // tree expected to already have new data
    // TODO: contingency for when node referenced by e.key is locked
    inline void entity_model::m_update_indexes(const data_node_layout_changes& changes)
    {
        const auto& [old_indexes, new_indexes] = changes;

        for (const QModelIndex& idx : new_indexes) // all indexes are for entities that also occur as nodes in the tree
        {
            const reflexive_entity& e = m_entity_for_index(idx);

            m_tree[e.key()].set_index(idx.row());
        }
        changePersistentIndexList(old_indexes, new_indexes);
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
        const QString parent_id = m_tree[parent].const_descriptor().text();
        e.attribute<QString>(Parent_Id) = parent_id;

        if (e.type() == sz::entity_type::File && m_tree[parent].const_descriptor().source() == query_source::Filesystem)
        {
            QFileInfo info = e.attribute<QFileInfo>();
            info.setFile(parent_id + QDir::separator() + e.attribute<QString>(Name));
            info.refresh();
            e.assign_attribute(File_Info, info);
        }
    }

    // assumes key exists in the tree
    inline void entity_model::m_update_size_attribute_for_item_representing_node(data_node_key key)
    {
        const auto& n = m_tree[key];
        const int pk = n.parent_key();
        const int i = n.index();
        if (m_tree.contains(pk) && i > -1) // if this node occurs as an item in another node
        {
            auto& e = m_tree[pk][i];
            e.attribute<QString>(Size) = QLocale::system().toString(n.const_descriptor().size()) + " items";
            const QModelIndex idx = m_index_for_entity(e, i);
            emit dataChanged(idx, idx);
        }
    }

    inline data_node_key entity_model::m_key_for_index(const QModelIndex& index) const
    {
        return static_cast<data_node_key>(index.internalId());
    }

    // field_names are not cleared here
    // this function would be called by resetInternalData(), which is called by QAbstractItemModel inside endResetModel()
    inline void entity_model::m_clear()
    {
        clear_thumbnails();
        m_dataman->clear();
        m_pending_queries.clear();
        m_pending_query_timer.stop();
        m_delayed_sort_timer.stop();
        m_tree.clear();
        m_items_pending_update.clear();
        m_root = std::nullopt;
    }

    // unwatches the path represented by key and stops any delayed queries associated with key
    inline void entity_model::m_deactivate(data_node_key key)
    {
        if (m_tree.contains(key))
        {
            m_dataman->set_watched(m_node_for_key(key).const_descriptor().uri(), false);
            m_remove_delayed_info_fetch(key);
            m_delayed_sort_timer.stop();
        }
    }

    inline void entity_model::m_schedule_delayed_info_fetch(data_node_key key, int start_index)
    {
        if (m_tree.contains(key) && start_index < m_tree[key].size())
        {
            m_delayed_info_fetches.emplace_back(key, start_index);
            m_delayed_info_fetch_timer.start(0, this);
        }
    }

    inline void entity_model::m_remove_delayed_info_fetch(data_node_key key, int start_index)
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

    inline void entity_model::m_schedule_delayed_sort(data_node_key key, int delay)
    {
        if (m_tree.contains(key))
        {
            m_delayed_sorts.push_back(key);
            m_delayed_sort_timer.start(delay, this);
        }
    }

    inline void entity_model::m_remove_delayed_sort(data_node_key key)
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

    inline void entity_model::m_schedule_delayed_query(const QString& uri)
    {
        m_pending_queries.push_back(uri);
        m_pending_query_timer.start(0, this);
    }

    inline void entity_model::m_remove_delayed_query(const QString& uri)
    {
        if (uri.isEmpty())
        { m_pending_queries.pop_front(); }
        else
        {
            auto new_end = std::remove(m_pending_queries.begin(), m_pending_queries.end(), uri);
            m_pending_queries.erase(new_end, m_pending_queries.end());
        }

        if (m_pending_queries.empty())
        { m_pending_query_timer.stop(); }
    }

    inline void entity_model::m_create_and_enqueue_node(const QString& uri, data_node_key parent_key, int index, std::optional<QList<QString>> uris)
    {
        using enum enqueue_position;
        using enum sz::entity_type;
        using namespace query_operator;

        assert(uri.size());

        query_descriptor qd(uri);
        qd.status = query_status::Initializing; // otherwise dataman will not set a size for this descriptor
        qd.source = qd.uri_type() == File ? query_source::Filesystem : (qd.uri_type() == Concept ? query_source::Concept : query_source::Index);
        
        if (m_sort_column && m_sort_order && std::cmp_less(*m_sort_column, m_attribute_section_positions.size()))
        {
            qd.sort_info = std::make_tuple(*m_sort_order, m_attribute_section_positions[*m_sort_column]);
            qd.keymap = m_tree.keymap();
        }
        const data_node_key k = m_tree.make_node(qd, parent_key, index);
        data_node opt = nullptr;
        
        if (uris)
        {
            // FIXME: use sz::entity_uri() to create the uri here
            concept_descriptor cd("concept://" + uri);
            cd.uris = *uris;
            cd.act = concept_descriptor::parameters::Ignore;
            opt = rin::make_data_node(cd);
        }
        m_dataman->enqueue(m_tree.data_node_for_key(k), opt, Read | Read_Extended | Sort, Front);
    }

    inline void entity_model::m_remove_node(data_node_key key)
    {
        qDebug() << "entity_model: remove node and all child nodes: " << m_uri_for_key(key);

        m_tree.traverse(key, [this](const entity_tree_node& node, int depth) -> bool
        {
            Q_UNUSED(depth);
            const QString uri = node.const_descriptor().uri();
            const int k = node.key();
            qDebug() << "entity_model: remove node: " << uri;

            m_dataman->set_watched(uri, false);
            m_dataman->dequeue(k);
            m_tree[k].set_state(entity_tree_node_state::Pending_Removal);

            beginRemoveRows(m_index_for_uri(uri), 0, node.size() - 1);

            if (m_root && *m_root == k)
            { m_root = std::nullopt; }

            m_tree.erase(k);

            endRemoveRows();

            return false;
        });
    }

    inline void entity_model::m_remove_entity(data_node_key parent_key, int index)
    {
        if (m_tree.contains(parent_key) && std::cmp_less(index, m_tree[parent_key].size()))
        {
            entity_tree_node& node = m_tree[parent_key];            
            std::unordered_map<QString, QModelIndex> old = m_indexes(node);
            QList<QModelIndex> old_indexes;
            QList<QModelIndex> new_indexes;
            old_indexes.reserve(node.size());
            new_indexes.reserve(node.size());
            old.erase(m_tree[parent_key][index].attribute<QString>(Name));
            
            beginRemoveRows(m_index_for_uri(node.const_descriptor().uri()), index, index);
            m_tree.erase(parent_key, index);

            int i = 0;
            for (const auto& name : node)
            {
                const auto& e = node[name];
                if (const data_node_key ek = e.key(); m_tree.contains(ek))
                {
                    m_tree[ek].set_index(i);
                }
                old_indexes << old[name];
                new_indexes << m_index_for_entity(e, i);
                ++i;
            }
            changePersistentIndexList(old_indexes, new_indexes);
            endRemoveRows();
        }
    }

    // calls m_remove_entity and m_remove_node if the entity also occurs as a node
    inline void entity_model::m_purge_entity_from_model(data_node_key parent_key, data_node_key entity_key, int entity_index)
    {
        if (m_tree.contains(entity_key))
        { m_remove_node(entity_key); }

        m_remove_entity(parent_key, entity_index);
    }

    inline std::unordered_map<QString, QModelIndex> entity_model::m_indexes(const entity_tree_node& node) const
    {
        std::unordered_map<QString, QModelIndex> indexes;
        indexes.reserve(node.size());
        for (int i = 0; i < node.size(); ++i)
        {
            const reflexive_entity& e = node[i];
            indexes.emplace(e.attribute<QString>(Name), m_index_for_entity(e, i));
        }
        return indexes;
    }

    inline QModelIndex entity_model::m_index_for_entity(const reflexive_entity& e, int node_index) const
    {
        assert(e.parent_key());
        return createIndex(node_index, 0, e.parent_key());
    }

    // assumes uri represents a node in the tree
    inline QModelIndex entity_model::m_index_for_uri(const QString& uri) const
    {
        const entity_tree_node& node = m_node_for_key(m_tree.key_for_uri(uri));

        // node.parent is not null if we have an index value
        const int idx = node.index();
        return createIndex(idx, 0, idx == -1 ? node.key() : node.parent_key());
    }

    inline entity_tree_node &entity_model::m_node_for_key(data_node_key key)
    {
        return m_tree[key];
    }

    inline const entity_tree_node& entity_model::m_node_for_key(data_node_key key) const
    {
        return m_tree[key];
    }

    inline const entity_tree_node& entity_model::m_node_for_uri(const QString& uri) const
    {
        return m_tree[uri];
    }

    // as the function name would suggest, bad things will happen if this is called with an invalid index
    inline const entity_tree_node& entity_model::m_node_for_valid_index(const QModelIndex& index) const
    {
        return m_tree[static_cast<data_node_key>(index.internalId())];
    }

    // returns the containing node for index or the root node if index is invalid
    // do not call this function if there is no root set
    inline const entity_tree_node& entity_model::m_node_for_index(const QModelIndex& index) const
    {
        if (const auto key = static_cast<data_node_key>(index.internalId()); key && m_valid_index(index))
        {
            return m_node_for_key(key);
        }
        return m_node_for_key(*m_root);
    }

    inline QString entity_model::m_uri_for_key(data_node_key key) const
    {
        return m_node_for_key(key).const_descriptor().uri();
    }

    // assumes the given index is valid
    inline QString entity_model::m_absolute_path_for_entity(const QModelIndex& index) const
    {
        const reflexive_entity& e = m_entity_for_index(index);

        if (e.has_attribute(File_Info))
        {
            return e.attribute<QFileInfo>().absoluteFilePath();
            // otherwise this is an entity whose information was pulled entirely from the database
        }
        return e.has_attribute(Id) ? e.attribute<QString>(Id) : QString();
    }

    // if the index is valid or partially-valid: returns a valid item id or the root's id (query string) respectively
    // if the entity is a file entity and it doesn't exist on the filesystem anymore, the model will attempt to update the id
    inline QString entity_model::m_id_for_index(const QModelIndex& index)
    {
        const entity_tree_node& node = m_node_for_index(index);
        const int i = index.row();

        if (node.contains(i))
        {
            if (node[i].type() == sz::entity_type::File)
            {
                QString id = node[i].attribute<QFileInfo>(File_Info).absoluteFilePath();
                if (!QFileInfo::exists(id))
                {
                    m_refresh_entity(m_tree[node.key()][i]);
                    id = node[i].attribute<QFileInfo>(File_Info).absoluteFilePath();
                }
                return id;
            }
            return node[i].attribute<QString>(Id);
        }
        else if (i == -1 && index.column() == 0) // either root or a former root
        {
            return node.const_descriptor().text();
        }
        
        return m_root ? m_node_for_key(*m_root).const_descriptor().text() : QString();
    }

    inline QString entity_model::m_id_for_index(const QModelIndex& index) const
    {
        if (index.isValid()) {
            return m_entity_for_index(index).attribute<QString>(Id);
        }
        if (m_valid_index(index)) {
            return m_node_for_index(index).const_descriptor().text();
        }
        return m_root ? m_node_for_key(*m_root).const_descriptor().text() : QString();
    }

    inline QString entity_model::m_uri_for_index(const QModelIndex& index) const
    {
        if (index.isValid())
        {
            const reflexive_entity& e = m_entity_for_index(index);
            if (const data_node_key ek = e.key(); m_tree.contains(ek)) {
                return m_uri_for_key(ek);
            }
            return QString::fromStdString(sz::entity_uri(m_id_for_index(index).toStdString(), e.type()));
        }
        if (m_valid_index(index)) {
            return m_node_for_index(index).const_descriptor().uri();
        }
        return m_root ? m_uri_for_key(*m_root) : QString();
    }

    // does not check if index is invalid
    inline reflexive_entity& entity_model::m_entity_for_index(const QModelIndex& index)
    {
        const data_node_key k = m_key_for_index(index);
        return m_node_for_key(k)[index.row()];
    }

    // does not check if index is invalid
    inline const reflexive_entity& entity_model::m_entity_for_index(const QModelIndex& index) const
    {
        return m_node_for_valid_index(index)[index.row()];
    }

    inline QModelIndex entity_model::m_reset_root_query(const QString& uri)
    {
        QModelIndex index;
        if (m_tree.contains(uri))
        {
            if (m_root)
            { m_deactivate(*m_root); }

            m_root = m_tree[uri].key();
            m_dataman->set_watched(uri, true);
            index = m_index_for_uri(uri);
        }
        return index;
    }

    inline bool entity_model::m_valid_index(const QModelIndex& index) const
    {
        const int r = index.row();
        const int c = index.column();
        const auto key = static_cast<data_node_key>(index.internalId());

        if (m_tree.contains(key))
        {
            const auto& node = m_node_for_key(key);
            return (r >= -1 && std::cmp_less(r, node.size())) && (c >= 0 && std::cmp_less(c, m_attribute_section_positions.size()));
        }
        return false;
    }

    inline bool entity_model::m_is_concept(const QModelIndex& index) const
    {
        if (m_valid_index(index))
        {
            if (index.row() == -1) {
                return m_node_for_index(index).const_descriptor().uri_type() == sz::entity_type::Concept;
            }
            return m_entity_for_index(index).type() == sz::entity_type::Concept;
        }
        return false;
    }

    inline bool entity_model::m_has_thumbnail_loaded(const QString& name, data_node_key node_key, sz::thumbnail_target_size size) const
    {
        return m_thumbnails.contains(node_key) &&
        m_thumbnails.at(node_key).contains(name) &&
        m_thumbnails.at(node_key).at(name).contains(size);
    }

    inline bool entity_model::m_has_thumbnail_loaded(const QString& name, data_node_key node_key) const
    {
        return m_has_thumbnail_loaded(name, node_key, m_thumbnail_target_size);
    }

    inline const QPixmap& entity_model::m_thumbnail_for_entity(const QString& name, data_node_key node_key, sz::thumbnail_target_size size) const
    {
        return m_thumbnails.at(node_key).at(name).at(size);
    }

    // does no checks
    inline const QPixmap& entity_model::m_thumbnail_for_entity(const QString& name, data_node_key node_key) const
    {
        return m_thumbnail_for_entity(name, node_key, m_thumbnail_target_size);
    }

    inline void entity_model::m_sort(data_node_key key, int column, Qt::SortOrder order)
    {
        if (column < 0 || column >= columnCount() || !m_tree.contains(key)) {
            return;
        }

        auto n = m_tree.data_node_for_key(key);
        
        n->set_sort_info(m_attribute_section_positions[column], order);
        m_dataman->enqueue(n, nullptr, query_operator::Sort);
    }
}
