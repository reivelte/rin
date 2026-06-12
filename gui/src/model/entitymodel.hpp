// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <unordered_map>
#include <vector>
#include <deque>
#include <stack>
#include <optional>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QSet>
#include <QtCore/QHash>
#include <QtCore/QMimeType>
#include <QtCore/QBasicTimer>
#include <QtWidgets/QFileIconProvider>
#include <QtGui/QIcon>
#include <suzuri/result.hpp>
#include "../rin.hpp"
#include "../utility/sizing.hpp"
#include "entity.hpp"
#include "tree.hpp"
#include "datamanager.hpp"
#include "historymanager.hpp"

namespace rin
{
    class entity_view;
    class entity_model : public QAbstractItemModel
    {
        Q_OBJECT

        public:
        struct item_properties
        {
            QString identifier;
            uintmax_t size;
            size_t dir_count;
            size_t file_count;
            size_t tag_count;

            item_properties() :
                identifier(), 
                size(0), dir_count(0), file_count(0), tag_count(0)
            {}

            item_properties(const query_descriptor& qd) :
                identifier(qd.full_text()),
                size(qd.filesize), dir_count(qd.dir_count), file_count(qd.file_count), tag_count(qd.tag_count)
            {}

            constexpr item_properties& operator+=(const item_properties& rhs) noexcept
            {
                size += rhs.size;
                dir_count += rhs.dir_count;
                file_count += rhs.file_count;
                tag_count += rhs.tag_count;
                return *this;
            }
        };

        public:
        entity_model(QObject* parent, const std::filesystem::path& database_path);
        ~entity_model();

        void set_batchsize(size_t size);
        void set_readonly(bool readonly);
        void set_database(const std::filesystem::path& path);
        
        QModelIndex query(const QString& text);
        QModelIndex query(const QString& text, const QList<QUrl>& urls);

        void tag(const QModelIndex& index, const std::vector<reflexive_entity>& tag_entities);
        void tag(const QModelIndex& index, const sz::metadata::parameters& args);
        void tag(const QString& entity_id, const sz::metadata::parameters& args);
        
        sz::result<QModelIndex> step(bool forward = true);
        void watch(const QModelIndex& index);
        void unwatch(const QModelIndex& index);
        
        bool remove(const QModelIndex& index, bool recycle = true);
        
        void clear_thumbnails();
        void request_thumbnails(const std::vector<QModelIndex>& indexes, const std::vector<QSize>& sizes);
        
        bool extended_information_loaded(const QModelIndex& index) const;
        QPixmap thumbnail(const QModelIndex& index) const;
        QPixmap thumbnail(const reflexive_entity& e) const;
        bool has_thumbnail(const QModelIndex& index) const;
        bool thumbnail_loaded(const QModelIndex& index) const;
        QModelIndex root_index() const;
        QString root_query() const;
        QString id_for_index(const QModelIndex& index) const;
        const reflexive_entity& at(const QModelIndex& index);
        item_properties properties(const QModelIndex& parent) const;
        bool entity_exists(const std::string& entity_id);

        // reimplemented functions
        QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
        QModelIndex parent(const QModelIndex& child) const override;
        int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        int columnCount(const QModelIndex& parent = QModelIndex()) const override;
        bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;
        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
        bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
        QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
        bool setHeaderData(int section, Qt::Orientation orientation, const QVariant& value, int role = Qt::EditRole) override;
        QStringList mimeTypes() const override;
        QMimeData* mimeData(const QModelIndexList& indexes) const override;
        bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) const override;
        bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) override;
        Qt::DropActions supportedDropActions() const override;
        Qt::DropActions supportedDragActions() const override;
        void fetchMore(const QModelIndex& parent) override;
        bool canFetchMore(const QModelIndex& parent) const override;
        Qt::ItemFlags flags(const QModelIndex& index) const override;
        void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;
        QModelIndex buddy(const QModelIndex& index) const override;
        QModelIndexList match(const QModelIndex& start, int role, const QVariant& value, int hits = 1, Qt::MatchFlags flags = Qt::MatchFlags(Qt::MatchStartsWith|Qt::MatchWrap)) const override;
        QSize span(const QModelIndex& index) const override;
        QHash<int, QByteArray> roleNames() const override;

        signals:
        void query_progressed(QString query_string, QList<std::tuple<QString, int>> processed);
        void query_failed(QString query_string, QList<std::tuple<QString, QString>> failed);
        void query_done(const QModelIndex& parent);
        void query_done(const QString& query_string);
        void need_thumbnails(const std::vector<std::tuple<QString, QModelIndex, QSize>>& data);
        void sort_starting();
        void sort_finished();
        void fetch_failed(const QString& target_path, const std::vector<entity_model_url>& urls, fetch_action action);
        
        public slots:
        void set_thumbnail_target_size(const QSize& size);
        void set_sort_preference(int column, Qt::SortOrder order);

        protected:
        // reimplemented functions
        void timerEvent(QTimerEvent* e) override;

        protected slots:
        void insert(const entity_tree_node& updated_node, int start, int end);
        void insert(const query_descriptor& qd, const std::vector<reflexive_entity>& data);
        void remove(const query_descriptor& qd, const std::vector<std::tuple<QString, int>>& to_remove);
        void invalidate(const QString& query_text);
        void invalidate(const QString& query_text, const QList<QUrl>& urls);
        void update(const query_descriptor& qd, std::vector<reflexive_entity> data);
        void update(const entity_tree_node& updated_node, int start, int end);
        void copy(const entity_tree_node& updated_node, const std::vector<node_index_change>& updated_key_indexes = {});
        void set_thumbnails(const std::vector<std::tuple<QString, QModelIndex, QPixmap>>& data);
        void finalize_query(query_descriptor qd);
        void handle_query_progress(query_descriptor qd, QList<std::tuple<QString, int>> processed);
        void handle_query_failure(query_descriptor qd, QList<std::tuple<QString, QString>> failed);
        void handle_fetch_progress(fetch_descriptor fd, std::vector<entity_model_url> urls_processed);
        void handle_fetch_failure(fetch_descriptor fd, std::vector<entity_model_url> urls);

        protected:
        friend class entity_view;

        struct item_temp_data
        {
            QString old_name;
            int index;
            fetch_action action;
        };

        protected:
        sz::result<entity_tree_node_state> node_state(const QModelIndex& parent) const;

        private:
        template <typename T> requires std::same_as<T, entity_tree_node> || std::same_as<T, std::vector<reflexive_entity>>
        inline void m_insert_entities(const QModelIndex& parent, int key, int start, int end, const T& data, bool copy_in);

        inline QIcon m_get_icon(const reflexive_entity& e);
        inline void m_refresh_entity(reflexive_entity& e);
        inline void m_update_size_attribute_for_item_representing_node(int key);
        inline int m_key_for_index(const QModelIndex& index) const;
        inline void m_clear();
        inline void m_deactivate(int key);
        inline void m_schedule_delayed_info_fetch(int key, int start_index);
        inline void m_remove_delayed_info_fetch(int key = 0, int start_index = -1);
        inline void m_schedule_delayed_sort(const int key, int delay);
        inline void m_remove_delayed_sort(const int key = 0);
        inline void m_interrupt_delayed_sort();
        inline void m_schedule_delayed_query(const QString& query_string);
        inline void m_remove_delayed_query(const QString& query_string = QString());
        inline void m_do_invalidate(entity_tree_node& node);
        inline void m_create_node(const QString& query_text, int parent_key, int index);
        inline void m_create_node(const QString& query_text, const QList<QUrl>& urls, int parent_key, int index);
        inline void m_remove_node(int key);
        inline void m_remove_item(int parent_key, int index);
        inline void m_purge_item(int parent_key, int item_key, int item_index);
        inline std::unordered_map<QString, QModelIndex> m_indexes(const entity_tree_node& node) const;
        inline QModelIndex m_index_for_item(const reflexive_entity& item, int pos) const;
        inline QModelIndex m_index_for_querytext(const QString& text) const;
        inline const entity_tree_node& m_node_for_key(int key) const;
        inline const entity_tree_node& m_node_for_valid_index(const QModelIndex& index) const;
        inline const entity_tree_node& m_node_for_index(const QModelIndex& index) const;
        inline QString m_id_for_key(int key) const;
        inline QString m_absolute_path_for_file_entity(const QModelIndex& index) const; // TODO
        inline QString m_id_for_index(const QModelIndex& index) const;
        inline const reflexive_entity& m_item_for_index(const QModelIndex& index) const;
        inline QModelIndex m_reset_root_query(const QString& text);
        inline bool m_valid_index(const QModelIndex& index) const;
        inline const QPixmap& m_thumbnail_for_index(const QModelIndex& index) const;
        inline void m_async_sort(int key, int column, Qt::SortOrder order);
        inline void m_async_sort(int key, entity_attribute_type sort_key, Qt::SortOrder sort_order);
        inline void m_sort(int key, int column, Qt::SortOrder order);
        
        private:
        std::unique_ptr<entity_data_manager> m_dataman;
        std::filesystem::path m_database_path;
        history_manager m_historyman;
        QThread m_thumbman;
        entity_tree m_tree;
        QHash<QMimeType, QIcon> m_icon_cache;
        QFileIconProvider m_icon_provider;
        std::unordered_map<int, std::unordered_map<QString, QPixmap>> m_thumbnails; // TODO: the thumbnail manager should hold this for us
        std::unordered_map<QString, std::unordered_map<QString, item_temp_data>> m_items_pending_update;
        std::unordered_map<QString, index_descriptor> m_in_progress_db_writes;
        std::vector<std::tuple<entity_attribute_type, QString>> m_field_names; // index is logical column value
        std::deque<QString> m_pending_queries; // TODO: this should be a tuple of string and query type
        std::deque<int> m_delayed_sorts;
        std::deque<std::tuple<int, int>> m_delayed_info_fetches; // key, start_index
        QBasicTimer m_pending_query_timer;
        QBasicTimer m_delayed_sort_timer;
        QBasicTimer m_delayed_info_fetch_timer;
        std::optional<int> m_root;
        std::optional<int> m_sort_column;
        std::optional<Qt::SortOrder> m_sort_order;
        QSize m_thumbnail_target_size;
        int m_batchsize;
        bool m_readonly;
    };

    template <typename T> requires std::same_as<T, entity_tree_node> || std::same_as<T, std::vector<reflexive_entity>>
    inline void entity_model::m_insert_entities(const QModelIndex& parent, int key, int start, int end, const T& data, bool copy_in)
    {
        using enum entity_attribute_type;
        using enum sz::entity_type;
        
        qDebug() << "entity_model: begin insert for: " << parent << ", start: " << start << ", end: " << end;

        if constexpr (std::same_as<T, entity_tree_node>)
        {
            if (copy_in)
            {
                beginInsertRows(parent, start, end);
                m_tree[key] = data;
                endInsertRows();
                return;
            }
        }        
        beginInsertRows(parent, start, end);
        entity_tree_node& node = m_tree[key];
        for (int i = start, j = 0; i <= end; ++i, ++j)
        {
            const int data_index = [&]() -> int { if constexpr (std::same_as<T, entity_tree_node>) { return i; } return j; }(); // lol
            
            reflexive_entity e = data[data_index];
            if (e.type() == File)
            {
                const auto& info = e.attribute<QFileInfo>();
                if (const QString id = info.absoluteFilePath(); m_tree.contains(id))
                {
                    const int child_key = m_tree.key_for_querytext(id);
                    m_tree[child_key].index = i;
                    m_tree[child_key].parent = key;
                    e.set_key(child_key);
                }

                if (e.has_attribute(Mime_Type) && !e.has_attribute(Icon))
                {
                    const QIcon icon = m_get_icon(e);
                    e.set_attribute(Icon, icon);
                }
            }
            node.insert(std::move(e));
        }
        endInsertRows();
    }

} // namespace rin