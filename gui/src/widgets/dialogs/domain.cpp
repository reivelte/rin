// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <QtCore/QRegularExpression>
#include <QtCore/QDir>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QFileDialog>
#include <QtGui/QRegularExpressionValidator>
#include "domain.hpp"

namespace rin
{
    domain_dialog::domain_dialog(QWidget* parent, domain_dialog_type type, const std::shared_ptr<sz::toml_config>& state_conf) :
        QDialog(parent), m_state_config(state_conf), m_type(type)
    {
        setModal(true);
        setSizeGripEnabled(true);
        setWindowTitle(tr("New Domain"));
        setMinimumSize(360, 240);
        // setMaximumSize(360, 240);

        // buttons
        m_button_box = new QDialogButtonBox(
            QDialogButtonBox::StandardButton::Cancel | QDialogButtonBox::StandardButton::Ok,
            Qt::Orientation::Horizontal,
            this
        );
        QPushButton* button = m_button_box->addButton("Choose Location...", QDialogButtonBox::ButtonRole::ActionRole);

        // text input
        m_name_field = new QLineEdit(this);
        m_name_field->setPlaceholderText(tr("(ex. bar.foo, baz, my.domain)"));
        m_name_field->setMaxLength(192);
        m_name_field->setValidator(new QRegularExpressionValidator(QRegularExpression("\\S+"), m_name_field));

        m_name_label = new QLabel(tr("&Name:"), this);
        m_name_label->setBuddy(m_name_field);

        m_name_input_layout = new QHBoxLayout;
        m_name_input_layout->addWidget(m_name_label);
        m_name_input_layout->addWidget(m_name_field);

        m_location_chooser = new QComboBox(this);
        m_location_chooser->setPlaceholderText(tr("Recent Domain Locations"));
        m_location_chooser->setSizeAdjustPolicy(QComboBox::SizeAdjustPolicy::AdjustToMinimumContentsLengthWithIcon);

        const std::filesystem::path default_loc = std::filesystem::path(sz::get_xdg_directory(sz::data_directory_type::Data)) / "rin" / "domains";
        if (!std::filesystem::exists(default_loc))
        {
            const bool ok = std::filesystem::create_directories(default_loc);
            if (!ok)
            {
                qWarning() << "domain_dialog: unable to create the default domain location. Failed to create directory: " << default_loc;
            }
        }
        if (std::filesystem::exists(default_loc))
        {
            m_location_chooser->addItem(QString::fromStdString(default_loc.string()));
            m_location_chooser->setCurrentText(QString::fromStdString(default_loc.string()));
        }

        const std::vector<std::filesystem::path> locations = m_state_config->domain_locations();
        for (const auto& x : locations)
        {
            m_location_chooser->addItem(QString::fromStdString(x.string()));
        }

        m_dialog_layout = new QVBoxLayout(this);
        m_dialog_layout->addLayout(m_name_input_layout);
        m_dialog_layout->addWidget(m_location_chooser);
        m_dialog_layout->addWidget(m_button_box);

        connect(m_button_box, &QDialogButtonBox::accepted, this, &domain_dialog::create_domain);
        connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(button, &QPushButton::clicked, this, &domain_dialog::choose_location);
    }

    domain_dialog::~domain_dialog()
    {
    }

    void domain_dialog::choose_location()
    {
        QFileDialog dialog(this, tr("Choose Location"), QString::fromStdString(sz::get_xdg_directory(sz::data_directory_type::Data)));
        dialog.setFileMode(QFileDialog::FileMode::Directory);
        if (dialog.exec())
        {
            const QString location = dialog.selectedFiles()[0];
            qDebug() << "domain_dialog: chosen location: " << location;
            m_location_chooser->addItem(location);
            m_location_chooser->setCurrentText(location);
            m_state_config->add_domain_location(location.toStdString());
        }
    }

    void domain_dialog::create_domain()
    {
        const QString name = m_name_field->text();
        const QString domain_path = m_location_chooser->currentText();
        if (name.size() && QFile::exists(domain_path))
        {
            const QString path = domain_path + QDir::separator() + name;
            qDebug() << "domain_dialog: name to use: " << name << ", in path: " << domain_path;
            if (!QFile::exists(path))
            {
                const bool ok = std::filesystem::create_directories(std::filesystem::path(path.toStdString()));
                if (!ok)
                {
                    qWarning() << "domain_dialog: failed to create domain: " << path;
                    reject();
                    return;
                }
            }
            emit domain_created(path);
            accept();
        }
        else
        {
            // show an error message in the dialog
            qDebug() << "domain_dialog: name or path is invalid";
        }
    }

} // namespace rin
