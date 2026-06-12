// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <QtWidgets/QFileDialog>
#include <QtWidgets/QPushButton>
#include <suzuri/utility/config_utils.hpp>
#include "tagger.hpp"

namespace rin
{
    tagger::tagger(QWidget* parent, entity_model* model) :
    QDialog(parent), m_model(model)
    {
        qDebug() << "[tagger] starting up";
        m_sidecar_dialog = nullptr;
        m_horizontal_splitter = new QSplitter(Qt::Horizontal, this);
        
        m_details_panel = new details_panel(this, model);
        
        m_view = new entity_view(this);
        m_view->setModel(m_model);
        
        m_tag_panel = new tag_panel(this, model);
        m_view_splitter = new QSplitter(Qt::Vertical, this);
        m_view_splitter->addWidget(m_view);
        m_view_splitter->addWidget(m_tag_panel);

        m_horizontal_splitter->addWidget(m_details_panel);
        m_horizontal_splitter->addWidget(m_view_splitter);
        
        m_buttons = new QDialogButtonBox(
            QDialogButtonBox::StandardButton::Cancel | QDialogButtonBox::StandardButton::Ok,
            Qt::Orientation::Horizontal,
            this
        );
        QPushButton* add_folder_button = m_buttons->addButton(tr("Add Folder..."), QDialogButtonBox::ButtonRole::AcceptRole);
        QPushButton* add_button = m_buttons->addButton(tr("Add Items..."), QDialogButtonBox::ButtonRole::ActionRole);
        QPushButton* sidecar_button = m_buttons->addButton(tr("From sidecar..."), QDialogButtonBox::ButtonRole::ActionRole);

        m_vlayout = new QVBoxLayout(this);
        m_vlayout->addWidget(m_horizontal_splitter);
        m_vlayout->addWidget(m_buttons);

        connect(add_folder_button, &QPushButton::clicked, this, &tagger::choose_folder);
        connect(add_button, &QPushButton::clicked, this, &tagger::choose_items);
        connect(sidecar_button, &QPushButton::clicked, this, &tagger::spawn_sidecar_dialog);
        connect(m_view, &entity_view::clicked, this, &tagger::set_current_item);
        connect(m_tag_panel, &tag_panel::tags_submitted, this, qOverload<const std::vector<reflexive_entity>&>(&tagger::submit_job));
        connect(m_model, &entity_model::query_progressed, this, &tagger::handle_query_progress);
        // TODO: create handler for failed query items
    }

    tagger::~tagger()
    {
    }

    void tagger::set_current_item(const QModelIndex& index)
    {
        using enum entity_attribute_type;
        m_selected_items = m_view->selected_indexes();
        const reflexive_entity& e = m_model->at(index);
        if (e.has_attribute(File_Info))
        {
            qDebug() << "[tagger] set current item to: " << e.attribute<QString>(Name) << ", path: " << e.attribute<QFileInfo>().absoluteFilePath();
            m_details_panel->set_item(e);
        }
    }

    void tagger::choose_folder()
    {
        QFileDialog dialog(this, tr("Choose Items"), QString::fromStdString(sz::get_home_directory().string()));
        dialog.setFileMode(QFileDialog::FileMode::Directory);

        // FIXME: this is duplicated in choose_items()
        if (dialog.exec())
        {
            QList<QUrl> urls;
            for (const auto& path : dialog.selectedFiles())
            {
                auto url = QUrl::fromLocalFile(path);
                urls.push_back(url);
                qDebug() << "[tagger] select: " << url;
            }
            QModelIndex index = m_model->query("concept://untitled", urls);
            m_view->setRootIndex(index);
        }
    }

    void tagger::choose_items()
    {
        QFileDialog dialog(this, tr("Choose Items"), QString::fromStdString(sz::get_home_directory().string()));
        dialog.setFileMode(QFileDialog::FileMode::ExistingFiles);
        if (dialog.exec())
        {
            QList<QUrl> urls;
            for (const auto& path : dialog.selectedFiles())
            {
                auto url = QUrl::fromLocalFile(path);
                urls.push_back(url);
                qDebug() << "[tagger] select: " << url;
            }
            QModelIndex index = m_model->query("concept://untitled", urls);
            m_view->setRootIndex(index);
        }
    }

    void tagger::submit_job(const std::vector<reflexive_entity>& tags)
    {
        if (m_selected_items.size() == 1)
        {
            const QModelIndex index = m_selected_items[0];
            m_model->tag(index, tags);
        }
    }

    // assumes entity_id refers to a valid entity in the database (for non-file entities) or a filesystem path
    void tagger::submit_job(const QString& entity_id, const sz::metadata::parameters& args)
    {
        m_model->tag(entity_id, args);
    }

    void tagger::handle_query_progress(QString query_string, QList<std::tuple<QString, int>> processed)
    {
        qDebug() << "[tagger] query: " << query_string << ", tagged items:";
        for (auto [item, tags_applied] : processed)
        {
            qDebug() << "[tagger] " << item << ", " << tags_applied << " tag(s) applied.";
        }
    }

    void tagger::spawn_sidecar_dialog()
    {
        if (m_sidecar_dialog)
        { delete m_sidecar_dialog; }

        if (m_selected_items.size())
        {
            QString path;
            for (const auto& index : m_selected_items)
            {
                auto e = m_model->at(index);
                if (e.has_attribute(File_Info))
                {
                    auto info = e.attribute<QFileInfo>();
                    if (info.isDir())
                    { path = info.absoluteFilePath(); }
                    break;
                }
            }
            m_sidecar_dialog = new sidecar_dialog(path, this);
            m_sidecar_dialog->show();
            connect(m_sidecar_dialog, &sidecar_dialog::accepted, this, &tagger::load_from_sidecar);
        }
    }

    void tagger::load_from_sidecar()
    {
        using enum sz::metadata::sidecar_filetype;
        
        auto items_path = m_sidecar_dialog->items_root();
        if (!(items_path.back() == "/"))
        {
            // we assume the user wants to recurse into the given directory if loading from sidecars
            items_path += "/";
        }

        auto path = m_sidecar_dialog->sidecar_root();
        auto split_string = m_sidecar_dialog->sidecar_split_string();
        auto keys = m_sidecar_dialog->sidecar_json_key();
        auto type = m_sidecar_dialog->sidecar_type();

        if (QFile::exists(items_path))
        {
            qDebug() << "[tagger] load tags from sidecars for item(s) in " << items_path;

            sz::metadata::parameters args{
                .tags{},
                .sidecar_keys = sz::utility::split(",", keys.toStdString()),
                .sidecar_split_character = split_string.toStdString(),
                .sidecar_alternate_root_path = std::nullopt,
                .sidecar_type = type == Json ? "json" : "txt",
                .use_sidecars = true,
                .check_tags = false
            };

            if (QFileInfo info(path); info.isDir())
            {
                qDebug() << "[tagger] use alternate sidecar root: " << path;
                args.sidecar_alternate_root_path = path.toStdString();
            }

            if (type == Json && args.sidecar_keys.empty())
            {
                qDebug() << "[tagger] no key specified for json sidecars";
                return;
            }
            submit_job(items_path, args);
        }
    }

} // namespace rin