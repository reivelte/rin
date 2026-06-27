// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <string>
#include <string_view>
#include <unordered_map>
#include <filesystem>
#include <QtCore/QDir>
#include <QtCore/QtLogging>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtGui/QDesktopServices>
#include <QtGui/QImageReader>
#include <QtGui/QFileSystemModel>
#include <QtGui/QContextMenuEvent>
#include "window.hpp"
#include "view/view.hpp"
#include "widgets/navigationpanel.hpp"
#include "widgets/iconsizer.hpp"
#include "widgets/dialogs/domain.hpp"

namespace rin
{
    main_window::main_window(QWidget* parent)
    : QMainWindow(parent)
    {
        setWindowTitle("rin-gui");
        m_domain_dialog = nullptr;
        m_settings_dialog = nullptr;
        m_navpanel = nullptr;
        m_details_panel = nullptr;
        m_tagger = nullptr;
        
        const auto local_state_dir = std::filesystem::path(sz::get_xdg_directory(sz::data_directory_type::State)) / "rin";
        bool state_config_dir_available = true;
        if (!std::filesystem::exists(local_state_dir))
        {
            const bool success = std::filesystem::create_directories(local_state_dir);
            if (!success)
            {
                qWarning() << "main_window: unable to access state directory for rin. no state settings will be loaded.";
                state_config_dir_available = false;
            }
        }

        if (state_config_dir_available)
        {
            std::filesystem::path state_config_path = local_state_dir / "state.toml";
            if (std::filesystem::exists(state_config_path))
            {
                qDebug() << "main_window: setting state config to: " << state_config_path;
                // m_state_config.set_config(state_config_path, sz::config_type::State);
                m_state_config = std::make_shared<sz::toml_config>(state_config_path, sz::config_type::State);
            }
            else
            {
                qDebug() << "main_window: could not find state config. trying to create one...";
                // try to create it
                // TODO
            }
            
            if (auto [w, h] = m_state_config->window_size(); w > 0 && h > 0)
            { resize(w, h); }
            else
            { resize(800, 600); }
        }

        const auto local_config_dir = std::filesystem::path(sz::get_xdg_directory(sz::data_directory_type::Config)) / "rin";
        if (!std::filesystem::exists(local_config_dir))
        {
            // TODO: try alternatives before resorting to creating a new directory
            const bool ok = std::filesystem::create_directories(local_config_dir);
            if (!ok)
            {
                qWarning() << "[main_window] unable to create config directory: " << local_config_dir;
                
            }
        }

        if (std::filesystem::exists(local_config_dir))
        {
            const auto config_path = local_config_dir / "gui.toml";
            m_main_config = std::make_shared<sz::toml_config>(config_path, sz::config_type::Static);
        }

        m_model = new entity_model(this, ":memory:"); // is this a good default for the database?
        m_model->set_batchsize(100000); // TODO: make configurable
        m_model->set_readonly(false);

        if (auto db_path = m_state_config->domain_recently_used(); sz::is_valid_domain(db_path))
        {
            m_set_database(db_path);
        }
        else
        {
            m_set_database(":memory:");
        }
        qDebug() << "main_window: using domain database: " << m_database_path;
        
        m_view = new entity_view(this);
        m_view->set_viewmode(entity_view_mode::List);
        m_view->setModel(m_model);
        
        m_lineedit = new QLineEdit;
        m_location_bar = new QToolBar("LocationBar", this);
        m_location_bar->setFloatable(false);
        m_location_bar->setMovable(false);
        m_location_bar->addWidget(m_lineedit);
        addToolBar(Qt::ToolBarArea::TopToolBarArea, m_location_bar);
        
        m_navpanel = new navigation_panel(this);
        if (auto targets = m_main_config->value<std::vector<std::string>>("navigation_panel.targets"); targets.size())
        {
            for (const auto& path : targets)
            {
                m_navpanel->add_target(QFileInfo(QString::fromStdString(path)));
            }
        }
        
        m_nav_bar = new QToolBar("Navigation", this);
        m_nav_bar->setAllowedAreas(Qt::ToolBarArea::LeftToolBarArea | Qt::ToolBarArea::RightToolBarArea);
        m_nav_bar->addWidget(m_navpanel);
        addToolBar(Qt::ToolBarArea::LeftToolBarArea, m_nav_bar);

        m_details_panel = new details_panel(this, m_model);
        
        m_details_bar = new QToolBar("Details", this);
        m_details_bar->setAllowedAreas(Qt::ToolBarArea::LeftToolBarArea | Qt::ToolBarArea::RightToolBarArea);
        m_details_bar->addWidget(m_details_panel);
        addToolBar(Qt::ToolBarArea::RightToolBarArea, m_details_bar);
        
        m_iconmode_toggle = new QToolButton;
        m_iconmode_toggle->setCheckable(true);
        m_iconmode_toggle->setAutoExclusive(true);
        m_iconmode_toggle->setDown(true);
        m_iconmode_toggle->setIcon(QIcon::fromTheme("view-list-icons"));
        
        m_listmode_toggle = new QToolButton;
        m_listmode_toggle->setCheckable(true);
        m_listmode_toggle->setAutoExclusive(true);
        m_listmode_toggle->setDown(false);
        m_listmode_toggle->setIcon(QIcon::fromTheme("view-list-tree"));

        m_icon_sizer = new icon_sizer(this);
        m_icon_sizer->setValue(m_view->iconSize().height());
        
        m_status_bar = statusBar();
        m_status_bar->addPermanentWidget(m_iconmode_toggle);
        m_status_bar->addPermanentWidget(m_listmode_toggle);
        m_status_bar->addPermanentWidget(m_icon_sizer);

        setCentralWidget(m_view);

        connect(m_lineedit, &QLineEdit::returnPressed, this, &main_window::read_input);
        connect(m_view, &entity_view::clicked, this, &main_window::select_item);
        connect(m_view, &entity_view::navigate_request, this, &main_window::navigate);
        connect(m_view, &entity_view::activated, this, &main_window::open_item);
        connect(m_view, &entity_view::selected_count_changed, this, &main_window::update_status_view_selected_counts);
        connect(m_view, &entity_view::iconSizeChanged, this, &main_window::update_iconsizer);
        connect(m_view, &entity_view::rightclicked, this, &main_window::handle_view_rightclick);
        connect(m_view, &entity_view::expanded, this, &main_window::update_status_view_counts);
        connect(m_view, &entity_view::collapsed, this, &main_window::update_status_view_counts);
        connect(m_model, &entity_model::rowsInserted, this, &main_window::update_status_view_counts);
        connect(m_model, qOverload<const QModelIndex&>(&entity_model::query_done), this, &main_window::update_status_view_counts);

        // TODO: to support multiple views + one model, items need to be sorted by making use of proxy models
        connect(m_view, &entity_view::sort_indicator_changed, m_model, &entity_model::set_sort_preference);

        connect(m_iconmode_toggle, &QAbstractButton::clicked, this, [this](bool clicked) -> void
        {
            if (clicked)
            {
                m_view->set_viewmode(entity_view_mode::Icon);
                update_status_view_counts(QModelIndex());
            }
        });
        connect(m_listmode_toggle, &QAbstractButton::clicked, this, [this](bool clicked) -> void
        {
            if (clicked)
            {
                m_view->set_viewmode(entity_view_mode::List);
                update_status_view_counts(QModelIndex());
            }
        });
        connect(m_icon_sizer, &icon_sizer::valueChanged, this, &main_window::set_view_iconsize);
        connect(m_navpanel, &navigation_panel::item_clicked, this, &main_window::navigate_to_navpanel_item);
        
        m_create_menubar();

        m_model->set_sort_preference(0, Qt::SortOrder::DescendingOrder);

        QString path;
        if (auto start_folder = m_main_config->value<std::string>("general.start_folder"); start_folder.size())
        { path = QString::fromStdString(start_folder); }
        else
        { path = QString::fromStdString(sz::get_home_directory().string()); }

        m_query(path);
    }

    main_window::~main_window()
    {
        if (m_state_config && m_state_config->available())
        {
            QSize s = size();
            m_state_config->set_window_size(s.width(), s.height());
        }
    }

    void main_window::update_status_view_counts(const QModelIndex& index)
    {
        Q_UNUSED(index);
        
        qDebug() << "window: update status: view counts";
        entity_model::item_properties props;
        props.identifier = m_model->root_query();
        for (const QModelIndex& idx : m_view->expanded_indexes())
        {
            props += m_model->properties(idx);
        }
        m_update_view_stats(props);
    }

    void main_window::update_status_view_selected_counts(qsizetype count)
    {
        if (count)
        {
            uintmax_t size = 0;
            size_t dirs_selected = 0;
            size_t files_selected = 0;
            size_t tags_selected = 0;
            for (const QModelIndex& index : m_view->selected_indexes())
            {
                // root is never selected, so we dont need to check for that here
                auto item = m_model->at(index);
                if (item.type() == sz::entity_type::File)
                {
                    const QFileInfo info = item.attribute<QFileInfo>(entity_attribute_type::File_Info);
                    if (info.isDir())
                    { ++dirs_selected; }
                    else
                    { ++files_selected; }
                    size += static_cast<uintmax_t>(info.size());
                }
                else if (item.type() == sz::entity_type::Tag || item.type() == sz::entity_type::Alias)
                {
                    ++tags_selected;
                }
            }

            const QString m = m_make_view_stats_status_message(size, dirs_selected, files_selected, tags_selected)
            + " selected.";
            m_status_bar->showMessage(m);
        }
        else
        { m_status_bar->showMessage(m_status_text); }
    }

    void main_window::read_input()
    {
        m_query(m_lineedit->text());
    }

    void main_window::navigate_to_navpanel_item(int item_index)
    {
        // item expected to be valid here
        const auto item = m_navpanel->item_at(item_index);
        m_query(item.info.absoluteFilePath());
    }

    void main_window::navigate(const QModelIndex& index, bool forward)
    {
        Q_UNUSED(index);
        if (const sz::result<QModelIndex> root = m_model->step(forward); root)
        { m_set_root(*root); }
        else
        { qDebug() << "main_window::navigate: step not valid"; }
    }

    void main_window::open_item(const QModelIndex& index)
    {
        if (const QFileInfo info = m_model->at(index).attribute<QFileInfo>(entity_attribute_type::File_Info); info.isDir())
        {
            m_query(info.absoluteFilePath());
        }
        else if (info.isFile())
        {
            if (const QUrl url = QUrl::fromLocalFile(info.absoluteFilePath()); QDesktopServices::openUrl(url)) 
            { qDebug() << "Using default program for" << url; }
            else
            { qDebug() << "could not open url: " << url; }
        }
        else
        { qDebug() << "Not a file"; }
    }

    void main_window::edit_item(const QModelIndex& index)
    {
        m_view->setCurrentIndex(index);
        m_view->edit(index);
    }

    void main_window::set_view_iconsize(int size)
    {
        m_view->set_iconsize(QSize(size, size));
    }

    void main_window::update_iconsizer(const QSize& size)
    {
        m_icon_sizer->setValue(size.height());
    }

    // TODO: handle when multiple items are selected in view
    void main_window::handle_view_rightclick(const QModelIndex& index)
    {
        QMenu menu(this);
        menu.addAction(m_actions[Cut]);
        menu.addAction(m_actions[Copy]);
        menu.addAction(m_actions[Paste]);
        
        if (index.isValid())
        {
            QAction* action_edit = new QAction(QIcon::fromTheme(QIcon::ThemeIcon::DocumentPageSetup), tr("&Edit"), &menu);
            connect(action_edit, &QAction::triggered, this, [this, index](bool checked) -> void
            {
                Q_UNUSED(checked);
                edit_item(index);
            });
            menu.addAction(action_edit);

            // TODO: to support changing the deletion method dynamically like in Dolphin, we would need to subclass QMenu
            // and reimplement its keyPressEvent
            if (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier)
            {
                QAction* action_del = new QAction(QIcon::fromTheme(QIcon::ThemeIcon::EditDelete), tr("&Delete"), &menu);
                connect(action_del, &QAction::triggered, this, [this, index](bool checked) -> void
                {
                    Q_UNUSED(checked);
                    m_model->remove(index, false);
                });
                menu.addAction(action_del);
            }
            else
            {
                QAction* action_recycle = new QAction(QIcon::fromTheme("user-trash"), tr("&Move to Trash"), &menu);
                connect(action_recycle, &QAction::triggered, this, [this, index](bool checked) -> void
                {
                    Q_UNUSED(checked);
                    m_model->remove(index);
                });
                menu.addAction(action_recycle);
            }
        }
        menu.exec(QCursor::pos());
    }

    void main_window::reset_navigation_panel()
    {
        m_navpanel->clear();
        if (auto paths = m_main_config->value<std::vector<std::string>>("navigation_panel.targets"); paths.size())
        { m_navpanel->add_targets(paths); }
    }

    void main_window::handle_tagger_close()
    {
        qDebug() << "main_window: tagger is closing down";
        m_tagger->deleteLater();
        m_tagger = nullptr;
    }

    void main_window::create_domain(const QString &path)
    {
        qDebug() << "main_window: create domain: " << path;
        auto db_path = std::filesystem::path(path.toStdString()) / (QDir(path).dirName().toStdString() + ".db");
        m_set_database(db_path);
    }

    void main_window::new_domain()
    {
        // prompt for name and path to save domain to
        // ???
        if (m_domain_dialog)
        {
            m_domain_dialog->deleteLater();
        }
        m_domain_dialog = new domain_dialog(this, domain_dialog_type::New, m_state_config);
        connect(m_domain_dialog, &domain_dialog::domain_created, this, &main_window::create_domain);
        m_domain_dialog->show();
    }

    // not to be used to implicitly create a database
    void main_window::open_domain()
    {
        // TODO: use recently used domain location
        const auto default_domain_location = std::filesystem::path(sz::get_xdg_directory(sz::data_directory_type::Data)) / "rin" / "domains";
        QFileDialog dialog(this, tr("Open Domain"), QString::fromStdString(default_domain_location.string()));
        dialog.setFileMode(QFileDialog::FileMode::Directory);
        if (dialog.exec())
        {
            const QString dir_path = dialog.selectedFiles()[0];
            const QString domain_name = QDir(dir_path).dirName();
            const QString db_name = domain_name + ".db";
            // const QString conf_name = domain_name + ".toml";
            if (QFile::exists(dir_path + QDir::separator() + db_name))
            {
                qDebug() << "main_window: open domain at path: " << dir_path;
                // TODO: verify integrity/validity of domain at path
                auto db_path = (dir_path + QDir::separator() + db_name).toStdString();
                m_set_database(db_path);
            }
        }
    }

    void main_window::open_settings()
    {
        qDebug() << "rin settings";
        if (m_settings_dialog)
        { delete m_settings_dialog; }

        m_settings_dialog = new settings_dialog(m_main_config, this);
        connect(
            m_settings_dialog->navigation_settings_page(), 
            &navigation_panel_settings_form::new_navigation_panel_targets_applied, 
            this, &main_window::reset_navigation_panel
        );
        m_settings_dialog->show();
    }

    void main_window::quit()
    {
    }

    void main_window::undo()
    {
    }

    void main_window::redo()
    {
    }

    void main_window::cut()
    {
    }

    void main_window::copy()
    {
    }

    void main_window::paste()
    {
    }

    void main_window::open_tagger()
    {
        if (m_tagger)
        {
            m_tagger->deleteLater();
            m_tagger = nullptr;
        }
        m_tagger = new tagger(this, m_model);
        connect(m_tagger, &tagger::about_to_close, this, &main_window::handle_tagger_close);
        m_tagger->show();
    }

    void main_window::about()
    {
        QMessageBox msgbox(
            QMessageBox::Icon::Information,
            tr("About Rin"),
            tr("Rin is a tag-centric file browser and data manager."),
            QMessageBox::StandardButton::Ok,
            this
        );

        auto* btn = msgbox.addButton(tr("About Qt"), QMessageBox::ButtonRole::ActionRole);
        connect(btn, &QPushButton::clicked, this, [&]() -> void
        {
            QMessageBox::aboutQt(this);
        });

        msgbox.exec();
    }

    void main_window::select_item(const QModelIndex& index)
    {
        auto e = m_model->at(index);
        m_details_panel->set_item(e);
    }

    void main_window::contextMenuEvent(QContextMenuEvent* e)
    {
        QMainWindow::contextMenuEvent(e);
    }

    void main_window::m_query(const QString& text)
    {
        if (const QModelIndex root = m_model->query(text); m_model->valid_index(root))
        {
            m_lineedit->setText(m_model->root_query());
            m_view->setRootIndex(root);
            m_set_window_title();
            m_update_view_stats({});
        }
    }

    void main_window::m_set_window_title()
    {
        auto title = std::format("[{}] {}", m_database_path.stem().string(), m_model->root_query().toStdString());
        setWindowTitle(QString::fromStdString(title));
    }

    void main_window::m_set_database(const std::filesystem::path& path)
    {
        m_database_path = path;
        m_model->set_database(path);
        m_state_config->set_recently_used_domain(path);
        m_set_window_title();
    }

    void main_window::m_set_root(const QModelIndex& index)
    {
        m_view->setRootIndex(index);
        m_lineedit->setText(m_model->id_for_index(index));
        update_status_view_counts(QModelIndex());
    }

    QString main_window::m_make_view_stats_status_message(uintmax_t size, size_t dir_count, size_t file_count, size_t tag_count)
    {
        const QString tags = tag_count == 1 ? "tag" : "tags";
        const QString dirs = dir_count == 1 ? "folder" : "folders";
        const QString files = file_count == 1 ? "file" : "files";
        const QString readable_size = QLocale::system().formattedDataSize(size);
        
        QString m;
        if (tag_count)
        {
            m += tr("%1 %2").arg(tag_count).arg(tags);
        }
        if (dir_count)
        {
            if (tag_count)
            { m += ", "; }
            m += tr("%1 %2").arg(dir_count).arg(dirs);
        }
        if (file_count)
        {
            if (dir_count)
            { m += ", "; }
            m += tr("%1 %2 (%3)").arg(file_count).arg(files).arg(readable_size);
        }
        return m;
    }

    void main_window::m_update_view_stats(const entity_model::item_properties& props)
    {
        m_status_text = m_make_view_stats_status_message(props.size, props.dir_count, props.file_count, props.tag_count);
        m_status_bar->showMessage(m_status_text);
        update_status_view_selected_counts(m_view->selected_indexes().size());
    }

    void main_window::m_create_menubar()
    {
        using enum window_menu_type;
        static constexpr std::array<window_action_data, 11> action_data
        {
            window_action_data{File, "&New",   "Create a new tag domain",   QIcon::ThemeIcon::DocumentNew,      QKeySequence::New,          &main_window::new_domain},
            window_action_data{File, "&Open",  "Open a tag domain",         QIcon::ThemeIcon::DocumentOpen,     QKeySequence::Open,         &main_window::open_domain},
            window_action_data{File, "&Settings", "Configure program behavior", {}, {}, &main_window::open_settings},
            window_action_data{File, "&Exit",  "Exit rin",                  QIcon::ThemeIcon::ApplicationExit,  QKeySequence::Quit,         &main_window::quit},
            window_action_data{Edit, "&Undo",  "Undo the previous action",  QIcon::ThemeIcon::EditUndo,         QKeySequence::Undo,         &main_window::undo},
            window_action_data{Edit, "&Redo",  "Redo the previous action",  QIcon::ThemeIcon::EditRedo,         QKeySequence::Redo,         &main_window::redo},
            window_action_data{Edit, "&Cut",   "Cut",                       QIcon::ThemeIcon::EditCut,          QKeySequence::Cut,          &main_window::cut},
            window_action_data{Edit, "&Copy",  "Copy",                      QIcon::ThemeIcon::EditCopy,         QKeySequence::Copy,         &main_window::copy},
            window_action_data{Edit, "&Paste", "Paste",                     QIcon::ThemeIcon::EditPaste,        QKeySequence::Paste,        &main_window::paste},
            window_action_data{Edit, "&Tag", "Tag files, folders, and other entities", QIcon::ThemeIcon::FolderOpen, QKeySequence::UnknownKey, &main_window::open_tagger},
            window_action_data{Help, "&About", "About rin",                 QIcon::ThemeIcon::HelpAbout,        QKeySequence::HelpContents, &main_window::about}
        };

        m_file_menu = menuBar()->addMenu(tr("&File"));
        m_edit_menu = menuBar()->addMenu(tr("&Edit"));
        m_help_menu = menuBar()->addMenu(tr("&Help"));

        size_t i = 0;
        for (const auto [type, string, desc, icon, keyseq, fn] : action_data)
        {
            QAction*& action = m_actions[i];
            action = new QAction(QIcon::fromTheme(icon), tr(string), this);
            action->setShortcuts(keyseq);
            action->setStatusTip(tr(desc));
            connect(action, &QAction::triggered, this, fn);

            switch (type)
            {
                case File:
                {
                    if (std::string_view(string) == "&Exit")
                    { m_file_menu->addSeparator(); }

                    m_file_menu->addAction(action);
                    break;
                }
                case Edit:
                {
                    if (std::string_view(string) == "&Cut")
                    { m_edit_menu->addSeparator(); }

                    m_edit_menu->addAction(action);
                    break;
                }
                case Help:
                {
                    m_help_menu->addAction(action);
                    break;
                }
                default: assert(false);
            }
            ++i;
        }

    }

} // namespace rin
