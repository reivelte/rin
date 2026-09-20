#pragma once

#include <memory>
#include <QtWidgets/QDialog>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QFileDialog>
#include <suzuri/utility/config_utils.hpp>

namespace rin
{
    class navigation_panel_settings_form;
    class general_settings_form;
    class view_settings_form;

    class settings_dialog : public QDialog
    {
        Q_OBJECT
        
        public:
        settings_dialog(const std::shared_ptr<sz::toml_config>& config, QWidget* parent);
        ~settings_dialog();

        general_settings_form* general_settings_page() const { return m_general_settings; }
        navigation_panel_settings_form* navigation_settings_page() const { return m_navpanel_settings; }

        signals:
        void settings_applied();

        public slots:
        void change_page(QListWidgetItem* page_name_widget);
        bool apply_settings();
        void apply_settings_and_accept();

        protected slots:
        void handle_button_click(QAbstractButton* button);

        private:
        std::shared_ptr<sz::toml_config> m_config;
        // QHBoxLayout* m_hbox;

        general_settings_form* m_general_settings;
        navigation_panel_settings_form* m_navpanel_settings;
        view_settings_form* m_view_settings;

        QSplitter* m_hsplit;
        QVBoxLayout* m_vbox;
        QListWidget* m_menulist;
        QStackedWidget* m_pages;
        QDialogButtonBox* m_button_box;
        QPushButton* m_apply_button;

    };
} // namespace rin
