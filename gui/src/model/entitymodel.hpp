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
#include <suzuri/utility/thumbnail.hpp>
#include "../rin.hpp"
#include "../utility/sizing.hpp"
#include "entity.hpp"
#include "tree.hpp"
#include "datamanager.hpp"
#include "thumbnailmanager.hpp"

namespace rin
{
    class entity_view;
    class entity_model : public QAbstractItemModel
    {
        Q_OBJECT

        public: // constructors/operators
        entity_model(const std::filesystem::path& database_path, QObject* parent);
        ~entity_model();

        public: // setters
        void set_batchsize(usize size);
        void set_readonly(bool readonly);
        void set_database(const std::filesystem::path& path);
        void set_display_attributes(const std::vector<entity_attribute_type>& attrs);
        void set_display_attribute_string(entity_attribute_type attr, QString str);
        void set_watched(const QModelIndex& index, bool watched);
        
        public: // mutators
        QModelIndex query(QString text, bool replace_root = true);
        void sort(const QModelIndex& parent, int column, Qt::SortOrder order);
        void tag(const QModelIndex& index, const std::vector<reflexive_entity>& tag_entities);
        void tag(const QModelIndex& index, const sz::metadata::parameters& args);
        void tag(const QString& entity_id, const sz::metadata::parameters& args);
        void append_data(const QModelIndex& parent, const std::vector<reflexive_entity>& data);
        bool remove(const QModelIndex& index, bool recycle = true);
        void clear(const QModelIndex& parent);
        void clear_thumbnails();
        void request_thumbnails(const QList<QModelIndex>& indexes, sz::thumbnail_target_size size);

        public: // getters
        bool valid_index(const QModelIndex& index) const;
        bool extended_information_loaded(const QModelIndex& index) const;
        QPixmap thumbnail(const reflexive_entity& e, sz::thumbnail_target_size size) const;
        QPixmap thumbnail(const reflexive_entity& e) const;
        QPixmap thumbnail(const QModelIndex& index) const;
        bool has_thumbnail(const QModelIndex& index) const;
        bool has_thumbnail_loaded(const QModelIndex& index, sz::thumbnail_target_size size) const;
        bool has_thumbnail_loaded(const QModelIndex& index) const;
        QModelIndex root_index() const;
        QModelIndex index(const QString& uri);
        QString root_uri() const;
        QString id_for_index(const QModelIndex& index) const;
        const reflexive_entity& at(const QModelIndex& index);
        bool entity_exists(const std::string& entity_id) const;
        reflexive_entity entity_from_id(const std::string& id) const;

        public: // reimplemented setters
        bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
        bool setHeaderData(int section, Qt::Orientation orientation, const QVariant& value, int role = Qt::EditRole) override;

        public: // reimplemented mutators
        bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) override;
        void fetchMore(const QModelIndex& parent) override;
        void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

        public: // reimplemented getters
        QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
        QModelIndex parent(const QModelIndex& child) const override;
        int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        int columnCount(const QModelIndex& parent = QModelIndex()) const override;
        bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;
        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
        QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
        QStringList mimeTypes() const override;
        QMimeData* mimeData(const QModelIndexList& indexes) const override;
        bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) const override;
        Qt::DropActions supportedDropActions() const override;
        Qt::DropActions supportedDragActions() const override;
        bool canFetchMore(const QModelIndex& parent) const override;
        Qt::ItemFlags flags(const QModelIndex& index) const override;
        QModelIndex buddy(const QModelIndex& index) const override;
        QModelIndexList match(const QModelIndex& start, int role, const QVariant& value, int hits = 1, Qt::MatchFlags flags = Qt::MatchFlags(Qt::MatchStartsWith|Qt::MatchWrap)) const override;
        QSize span(const QModelIndex& index) const override;
        QHash<int, QByteArray> roleNames() const override;

        signals:
        void query_progressed(QString query_string, QList<std::tuple<QString, int>> processed);
        void query_failed(QString query_string, QList<std::tuple<QString, QString>> failed);
        void query_done(const QModelIndex& parent);
        void query_done(const QString& query_string);
        void sort_starting();
        void sort_finished();
        void thumbnail_available(const reflexive_entity& e, sz::thumbnail_target_size size);
        
        public slots:
        void set_thumbnail_target_size(sz::thumbnail_target_size size);
        void set_sort_preference(int column, Qt::SortOrder order);

        protected slots:
        void insert(data_node_update update);
        void invalidate(data_node_key k);
        void set_thumbnail(const thumbnail_descriptor& d);

        protected: // reimplemented functions
        void timerEvent(QTimerEvent* e) override;

        protected:
        friend class entity_view;
        friend class entity_data_manager;

        struct item_temp_data
        {
            QString old_name;
            int index;
            concept_descriptor::parameters::action action;
        };

        protected:
        sz::result<entity_tree_node_state> node_state(const QModelIndex& parent) const;
        QModelIndex create_index(const reflexive_entity& e, int idx) const;

        private:
        inline void m_update_indexes(const data_node_layout_changes& changes);
        inline QIcon m_get_icon(const reflexive_entity& e);
        inline void m_refresh_entity(reflexive_entity& e);
        inline void m_update_size_attribute_for_item_representing_node(data_node_key key);
        inline data_node_key m_key_for_index(const QModelIndex& index) const;
        inline void m_clear();
        inline void m_deactivate(data_node_key key);
        inline void m_schedule_delayed_info_fetch(data_node_key key, int start_index);
        inline void m_remove_delayed_info_fetch(data_node_key key = 0, int start_index = -1);
        inline void m_schedule_delayed_sort(data_node_key key, int delay);
        inline void m_remove_delayed_sort(data_node_key key = 0);
        inline void m_interrupt_delayed_sort();
        inline void m_schedule_delayed_query(const QString& uri);
        inline void m_remove_delayed_query(const QString& uri = QString());
        inline void m_create_and_enqueue_node(const QString& uri, data_node_key parent_key, int index, std::optional<QList<QString>> uris = std::nullopt);
        inline void m_remove_node(data_node_key key);
        inline void m_remove_entity(data_node_key parent_key, int index);
        inline void m_purge_entity_from_model(data_node_key parent_key, data_node_key entity_key, int entity_index);
        inline std::unordered_map<QString, QModelIndex> m_indexes(const entity_tree_node& node) const;
        inline QModelIndex m_index_for_entity(const reflexive_entity& e, int node_index) const;
        inline QModelIndex m_index_for_uri(const QString& uri) const;
        inline entity_tree_node& m_node_for_key(data_node_key key);
        inline const entity_tree_node& m_node_for_key(data_node_key key) const;
        inline const entity_tree_node& m_node_for_uri(const QString& uri) const;
        inline const entity_tree_node& m_node_for_valid_index(const QModelIndex& index) const;
        inline const entity_tree_node& m_node_for_index(const QModelIndex& index) const;
        inline QString m_uri_for_key(data_node_key key) const;
        inline QString m_absolute_path_for_entity(const QModelIndex& index) const;
        inline QString m_id_for_index(const QModelIndex& index);
        inline QString m_id_for_index(const QModelIndex& index) const;
        inline QString m_uri_for_index(const QModelIndex& index) const;
        inline reflexive_entity& m_entity_for_index(const QModelIndex& index);
        inline const reflexive_entity& m_entity_for_index(const QModelIndex& index) const;
        inline QModelIndex m_reset_root_query(const QString& uri);
        inline bool m_valid_index(const QModelIndex& index) const;
        inline bool m_is_concept(const QModelIndex& index) const;
        inline bool m_has_thumbnail_loaded(const QString& name, data_node_key node_key, sz::thumbnail_target_size size) const;
        inline bool m_has_thumbnail_loaded(const QString& name, data_node_key node_key) const;
        inline const QPixmap& m_thumbnail_for_entity(const QString& name, data_node_key node_key, sz::thumbnail_target_size size) const;
        inline const QPixmap& m_thumbnail_for_entity(const QString& name, data_node_key node_key) const;
        inline void m_sort(data_node_key key, int column, Qt::SortOrder order);
        
        private:
        using enum entity_attribute_type;
        std::unique_ptr<entity_data_manager> m_dataman;
        std::unique_ptr<thumbnail_manager> m_thumbman;
        std::filesystem::path m_database_path;
        entity_tree m_tree;
        QHash<QMimeType, QIcon> m_icon_cache;
        QFileIconProvider m_icon_provider;
        
        std::unordered_map<
            int, std::unordered_map<
                QString, std::unordered_map<
                    sz::thumbnail_target_size, QPixmap
                >
            >
        > m_thumbnails;
        
        std::unordered_map<QString, std::unordered_map<QString, item_temp_data>> m_items_pending_update;
        std::unordered_map<QString, index_descriptor> m_in_progress_db_writes;
        std::unordered_map<entity_attribute_type, QString> m_attribute_names; // key to string representation of key (user-modifiable)
        std::vector<entity_attribute_type> m_attribute_section_positions; // index is logical column value
        std::deque<QString> m_pending_queries; // TODO: this should be a tuple of string and query type
        std::deque<int> m_delayed_sorts;
        std::deque<std::tuple<int, int>> m_delayed_info_fetches; // key, start_index
        QBasicTimer m_pending_query_timer;
        QBasicTimer m_delayed_sort_timer;
        QBasicTimer m_delayed_info_fetch_timer;
        std::optional<data_node_key> m_root;
        std::optional<int> m_sort_column;
        std::optional<Qt::SortOrder> m_sort_order;
        sz::thumbnail_target_size m_thumbnail_target_size;
        int m_batchsize;
        bool m_readonly;
    };

} // namespace rin