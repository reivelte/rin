// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <memory>
#include <array>
#include <QtWidgets/QMainWindow>
#include <QtGui/QIcon>
#include <QtGui/QKeySequence>
#include <suzuri/utility/config_utils.hpp>
#include "tagger/tagger.hpp"
#include "widgets/dialogs/settings.hpp"
#include "widgets/details_panel.hpp"
#include "model/entitymodel.hpp"

QT_BEGIN_NAMESPACE
class QToolBar;
class QStatusBar;
class QLineEdit;
class QToolButton;
class QContextMenuEvent;
class QMenu;
class QAction;
QT_END_NAMESPACE

namespace rin
{
    class entity_view;
    class navigation_panel;
    class icon_sizer;
    class domain_dialog;
    class main_window : public QMainWindow
    {
        Q_OBJECT

        public:
        main_window(QWidget* parent = nullptr);
        ~main_window();

        public slots:
        // ###################################################### TODO: specific to a window tab ######
        // status message modifiers
        void update_status_view_counts(const QModelIndex& index);
        void update_status_view_selected_counts(qsizetype count);
        
        // model control + desktop services
        void read_input();
        void navigate_to_navpanel_item(int item_index);
        void navigate(const QModelIndex& index, bool forward);
        void open_item(const QModelIndex& index);
        void edit_item(const QModelIndex& index);
        
        // view control
        void set_view_iconsize(int size);
        void update_iconsizer(const QSize& size);
        void handle_view_rightclick(const QModelIndex& index);
        // ###################################################### specific to a window tab ######

        void reset_navigation_panel();
        void handle_tagger_close();
        void create_domain(const QString& path);
        
        // UI - menubar
        void new_domain();
        void open_domain();
        void open_settings();
        void quit();
        
        void undo();
        void redo();
        void cut();
        void copy();
        void paste();
        void open_tagger();
        
        void about();

        protected slots:
        void select_item(const QModelIndex& index);

        protected:
        void contextMenuEvent(QContextMenuEvent* e) override;

        private:
        enum class window_menu_type : int { File, Edit, Help };
        enum window_action_id : int { New = 0, Settings, Open, Exit, Undo, Redo, Cut, Copy, Paste, About };

        struct window_action_data
        {    
            window_menu_type type;
            const char* string;
            const char* description;
            QIcon::ThemeIcon icon;
            QKeySequence::StandardKey keyseq;
            void (main_window::*func)(void);
        };
        
        private:
        void m_query(const QString& text);
        void m_set_window_title();
        void m_set_database(const std::filesystem::path& path);
        void m_set_root(const QModelIndex& index);
        QString m_make_view_stats_status_message(uintmax_t size, size_t dir_count, size_t file_count, size_t tag_count);
        void m_update_view_stats(const entity_model::item_properties& props);
        void m_create_menubar();

        private:
        // application-wide
        std::shared_ptr<sz::toml_config> m_state_config;
        std::shared_ptr<sz::toml_config> m_main_config;
        std::filesystem::path m_database_path;
        QToolBar* m_nav_bar;
        QToolBar* m_details_bar;
        navigation_panel* m_navpanel;
        details_panel* m_details_panel;
        QMenu* m_file_menu;
        QMenu* m_edit_menu;
        QMenu* m_help_menu;
        std::array<QAction*, 11> m_actions;
        std::array<QAction*, 1> m_view_actions;
        domain_dialog* m_domain_dialog;
        tagger* m_tagger;
        settings_dialog* m_settings_dialog;

        
        // tab/window-wide
        QString m_status_text; // not for selected items
        QStatusBar* m_status_bar;
        QToolButton* m_iconmode_toggle;
        QToolButton* m_listmode_toggle;
        QToolBar* m_location_bar;
        icon_sizer* m_icon_sizer;
        QLineEdit* m_lineedit;
        entity_view* m_view;
        entity_model* m_model; // TODO: proxy models
        
    };
}
