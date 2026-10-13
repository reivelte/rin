// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <cstdint>
#include <optional>
#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtGui/QPixmap>
#include <suzuri/metadata/tags.hpp>
#include <suzuri/utility/thumbnail.hpp>

namespace rin
{
    struct entity_tree_node;
    class reflexive_entity;

    enum class entity_attribute_type : int
    {
        // applies to all entities
        Null_Attribute = 0,
        Name = 1,               // string
        Id,                     // string
        Parent_Id,              // string
        Tags,                   // set<string>
        Comments, Description,  // string
        Color,                  // int
        Url,                    // QUrl

        // applies to filesystem based entities
        Size, Modified, Created, Accessed, File_Type,       // string
        Size_Int, Modified_Int, Created_Int, Accessed_Int,  // qint64
        Rating,                                             // int
        Icon,                                               // QIcon
        File_Info,                                          // QFileInfo
        Mime_Type,                                          // QMimeType

        // applies to filesystem based image entities
        Sizehint,               // tuple<int, int> or similar

        // TODO: fields for specfic file types and entities
    };

    struct node_index_change
    {
        int key;
        int original_index;
        int new_index;
    };
    
    struct entity_model_url
    {
        QUrl src;
        QString target_name;
        int index;

        auto operator<=>(const entity_model_url&) const = default;
        QString to_target_path() const { return QFileInfo(src.toLocalFile()).absoluteDir().canonicalPath() + target_name; }
    };

    struct query_descriptor
    {
        enum class query_type : int8_t  { Null_Type, Concept, External, Filesystem, Indexed };
        enum class query_mode : int8_t  { Null_Mode, Read, Read_Extended, Fetch, Write, Sort };
        enum class query_status : int8_t { Invalid, Initializing, In_Progress, Completed, Paused, Canceled };

        QString text{};
        int64_t size = -1;
        int64_t processed = 0;
        int64_t processed_extended = 0;
        uintmax_t filesize = 0;
        size_t dir_count = 0;
        size_t file_count = 0;
        size_t tag_count = 0;
        query_type type = query_type::Null_Type;
        query_mode mode = query_mode::Null_Mode;
        query_status status = query_status::Invalid;
        std::optional<Qt::SortOrder> sort_order = std::nullopt;
        std::optional<entity_attribute_type> sort_key = std::nullopt;

        static QString full_text(const QString& text, query_descriptor::query_type type)
        {
            Q_UNUSED(type);
            // using enum query_type;
            // switch (type)
            // {
            // case Concept: return "rin://" + text + ".concept";
            // case Filesystem: return "file://" + text;
            // case Indexed: return "rin://" + text;
            // default: break;
            // }
            return text;
        }

        inline QString full_text() const
        {
            return full_text(text, type);
        }

        query_descriptor& operator+=(const query_descriptor& rhs)
        {
            // size += rhs.size;
            processed += rhs.processed;
            filesize += rhs.filesize;
            dir_count += rhs.dir_count;
            file_count += rhs.file_count;
            tag_count += rhs.tag_count;
            return *this;
        }

        // size is not modified here
        void adjust(const reflexive_entity& e, bool increment);
    };

    enum class fetch_action : unsigned int
    {
        // mask: 0xff
        Ignore = 0x0,
        Copy = Qt::DropAction::CopyAction, // 0x1 : 00000001
        Move = Qt::DropAction::MoveAction, // 0x2 : 00000010
        Link = Qt::DropAction::LinkAction, // 0x4 : 00000100
        Remove = 0x6, // 00001000
        Recycle = 0x8 // 00010000
    };

    struct fetch_descriptor
    {
        query_descriptor query{};
        std::vector<entity_model_url> urls{};
        fetch_action action{};
        bool is_update = false;
    };

    struct index_descriptor
    {
        query_descriptor query;
        sz::metadata::parameters args;
    };

    struct thumbnail_descriptor
    {
        QPixmap thumbnail;   // filled in by thumbnail_manager
        QString id;
        QString entity_name;
        QSize original_size; // only if the entity refers to a media file on disk (jpg, png, etc.), see entity.Sizehint
        sz::thumbnail_target_size target_size;
        sz::entity_type entity_type;
        int node_key;
    };

    void sort_entities(entity_tree_node& node);
    
} // namespace rin
