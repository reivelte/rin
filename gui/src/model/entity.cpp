// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <QtGui/QImageReader>
#include "entity.hpp"

namespace rin
{
    reflexive_entity reflexive_entity::from_uri_or_id(QString x, sz::entity_database* db, std::optional<QFileInfo> fsinfo)
    {
        using enum entity_attribute_type;
        using enum sz::entity_type;
        
        const std::string uri_ = x.toStdString();
        const auto [id_, type_from_uri] = sz::id_from_uri(uri_);
        const auto id = QString::fromUtf8(id_.data(), static_cast<qsizetype>(id_.size()));
        const auto info = db ? db->entity_info(std::string(id_)) : sz::entity_database_info();
        const bool file_exists = QFileInfo::exists(id);

        QString name;
        auto result_type = Invalid;

        if (x.size())
        {
            if (file_exists || (info.id.size() && (info.type == File)))
            {
                if (file_exists && !fsinfo)
                { fsinfo.emplace(id); }

                result_type = File;
            }
            else
            {
                if (info.id.size())
                { result_type = info.type; }
                else
                {
                    if (type_from_uri == Concept)
                    { result_type = Concept; }
                    else if (sz::is_valid_tag(id_)) // tag or alias, assumption is tag
                    { result_type = Tag; }
                }
            }

            if (result_type == File && file_exists)
            { name = fsinfo->fileName(); }
            else if (result_type == File)
            { name = QString::fromStdString(std::filesystem::path(info.id).filename().string()); }
            else if (result_type == Concept)
            { name = id; }
            else if (result_type == Tag)
            {
                // tag or alias
                // we will need to revisit this if we decide to support nested namespaces for tags
                const std::vector<std::string> s = sz::utility::split(":", id_);
                name = QString::fromStdString(s[s.size() > 1 ? 1 : 0]);
            }
        }

        // TODO: tag entities have children if additional tags occur within their inset
        // TODO: checking if non-file entities have a thumbnail available and setting the sizehint if so
        reflexive_entity e(name, result_type, info.id.size() > 0, result_type == File ? fsinfo->isDir() : false);
        e.set_attribute(Id, id);
        
        if (result_type == File && file_exists)
        {
            e.set_attribute(File_Info, *fsinfo);

            if (sz::supported_extensions().contains(fsinfo->suffix().toLower().toStdString()))
            { e.set_attribute(Sizehint, QImageReader(fsinfo->absoluteFilePath()).size()); }
        }

        return e;
    }

    // expects the Id attribute to already be set
    void reflexive_entity::fill_extended_attributes(sz::entity_database& db, const QMimeDatabase& mimedb, bool get_tag_set)
    {
        using enum sz::entity_type;
        const auto& id = attribute<QString>(Id);
        const auto t = type();

        if (id.size())
        {
            const bool file_exists = QFileInfo::exists(id);
            if (t == File && file_exists)
            {
                // this section does not work if the file/dir does not actually exist on the filesystem
                // the assumption then would be that it is an orphaned entity whose information is accessible from entity_database
                const bool has_stored_fileinfo = has_attribute(File_Info);
                const auto file = has_stored_fileinfo ? attribute<QFileInfo>() : QFileInfo(id);
                const auto mime = mimedb.mimeTypeForFile(file, QMimeDatabase::MatchExtension);
                set_attribute(Mime_Type, mime);
                set_attribute(Modified_Int, file.lastModified().toSecsSinceEpoch());
                set_attribute(Modified, file.lastModified().toString());
                set_attribute(Created_Int, file.birthTime().toSecsSinceEpoch());
                set_attribute(Created, file.birthTime().toString());
                set_attribute(Accessed_Int, file.lastRead().toSecsSinceEpoch());
                set_attribute(Accessed, file.lastRead().toString());
                set_attribute(File_Type, mime.comment().isEmpty() ? mime.name() : mime.comment());
                if (file.isFile())
                {
                    const qint64 s = file.size();
                    set_attribute(Size_Int, s);
                    set_attribute(Size, QLocale::system().formattedDataSize(s));
                }
                else
                {
                    qint64 s = 0;
                    QDirListing dir(id);
                    for (auto it = dir.begin(); it != dir.end(); ++it)
                    { ++s; }
                    set_attribute(Size_Int, s);
                    set_attribute(Size, QLocale::system().toString(s) + " " + QCoreApplication::translate("main_window", "items"));
                }
                
                if (!has_stored_fileinfo)
                { set_attribute(File_Info, file); }
            }

            if (const auto info = db.entity_info(id.toStdString()); info.id.size())
            {
                // TODO: look at extra values supplied based on entity type
                
                set_attribute(Description, QString::fromStdString(info.description));
                set_attribute(Color, info.rgba);

                if (get_tag_set && (t == File))
                {
                    sz::sqlite::database_query tags = db.tags(sz::entity_database_query{ .string = info.id });
                    auto& set = attribute<tag_set>();
                    
                    for (const std::string_view tag : tags)
                    { set.insert(QString::fromUtf8(tag.data(), static_cast<qsizetype>(tag.size()))); }
                }

                if ((t != File) || !file_exists)
                {
                    const auto ct = static_cast<qint64>(info.createtime);
                    const auto mt = static_cast<qint64>(info.modtime);
                    set_attribute(Created, QDateTime::fromSecsSinceEpoch(ct).toString());
                    set_attribute(Created_Int, ct);
                    set_attribute(Modified, QDateTime::fromSecsSinceEpoch(mt).toString());
                    set_attribute(Modified_Int, mt);
                    set_attribute(File_Type, QString::fromStdString(sz::utility::to_string(t)).toLower());
                }
            }
        }
    }

} // namespace rin
