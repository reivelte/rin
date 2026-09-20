#include <QtWidgets/QLineEdit>
#include <QtWidgets/QFileDialog>
#include "navigation_form.hpp"

namespace rin
{
    navigation_panel_settings_form::navigation_panel_settings_form(const std::shared_ptr<sz::toml_config>& config, settings_dialog* parent) :
        settings_form(parent), m_config(config)
    {
        m_form_layout = new QFormLayout(this);
        m_items = new QListWidget(this);

        m_path_input_layout = new QHBoxLayout;
        m_path_lineedit = new QLineEdit;
        m_path_chooser_button = new QPushButton(tr("Select Path..."));
        m_path_input_layout->addWidget(m_path_lineedit);
        m_path_input_layout->addWidget(m_path_chooser_button);

        m_home_lineedit = new QLineEdit;

        m_form_layout->addRow(tr("&Directories:"), m_items);
        m_form_layout->addRow(tr("Add Directory:"), m_path_input_layout);

        auto t = m_config->value<std::vector<std::string>>("navigation_panel.targets");
        if (t.size())
        {
            for (const std::string& path : t)
            {
                m_items->addItem(QString::fromStdString(path));
            }
        }

        connect(m_items, &QListWidget::itemActivated, this, &navigation_panel_settings_form::purge_item);
        connect(m_path_lineedit, &QLineEdit::returnPressed, this, &navigation_panel_settings_form::add_path_from_lineedit);
        connect(m_path_chooser_button, &QPushButton::clicked, this, &navigation_panel_settings_form::handle_button_click);
    }

    QList<QFileInfo> navigation_panel_settings_form::targets()
    {
        QList<QFileInfo> targets;
        for (int i = 0; i < m_items->count(); ++i)
        {
            // we expect to only get directory paths here
            auto* x = m_items->item(i);
            targets.emplace_back(x->text());
        }
        return targets;
    }

    bool navigation_panel_settings_form::can_apply_settings() const
    {
        return !(m_path_lineedit->hasFocus());
    }

    void navigation_panel_settings_form::purge_item(QListWidgetItem* item)
    {
        const int row = m_items->row(item);
        m_items->removeItemWidget(item);
        delete m_items->takeItem(row);
    }

    // TODO: all QFileDialogs need to use non-blocking show() instead of exec()
    void navigation_panel_settings_form::handle_button_click(bool checked)
    {
        Q_UNUSED(checked);
        QFileDialog dialog(this, tr("Select path"));
        dialog.setFileMode(QFileDialog::FileMode::Directory);
        if (dialog.exec())
        {
            const QString path = dialog.selectedFiles()[0];
            m_items->addItem(path);
        }
    }

    void navigation_panel_settings_form::commit()
    {
        auto t = targets();
        std::vector<std::string> paths;
        for (auto& info : t)
        {
            paths.emplace_back(info.absoluteFilePath().toStdString());
        }
        m_config->set_value("navigation_panel.targets", paths);
        emit new_navigation_panel_targets_applied();
    }

    void navigation_panel_settings_form::add_path_from_lineedit()
    {
        const QString path = m_path_lineedit->text();
        if (QFileInfo info(path); info.isDir())
        {
            m_items->addItem(path);
            m_path_lineedit->clear();
        }
    }
} // namespace rin
