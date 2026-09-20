#pragma once

#include <QtCore/QList>
#include <QtCore/QFileInfo>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QHBoxLayout>
#include "form.hpp"

namespace rin
{
    class navigation_panel_settings_form : public settings_form
    {
        Q_OBJECT
        
        public:
        navigation_panel_settings_form(const std::shared_ptr<sz::toml_config>& config, settings_dialog* parent);
        ~navigation_panel_settings_form() = default;

        QList<QFileInfo> targets();

        bool can_apply_settings() const;

        signals:
        void new_navigation_panel_targets_applied();
        void new_navigation_panel_targets(const QList<QFileInfo>& targets);
        
        public slots:
        void add_path_from_lineedit();
        void purge_item(QListWidgetItem* item);
        void handle_button_click(bool);
        
        void commit() override;

        private:
        std::shared_ptr<sz::toml_config> m_config;
        QFormLayout* m_form_layout;
        QListWidget* m_items;
        
        QHBoxLayout* m_path_input_layout;
        QLineEdit* m_path_lineedit;
        QPushButton* m_path_chooser_button;
        
        QLineEdit* m_home_lineedit;
    };
} // namespace rin
