#include "dialog.hpp"
#include "general_form.hpp"
#include "navigation_form.hpp"
#include "view_form.hpp"

namespace rin
{
    settings_dialog::settings_dialog(const std::shared_ptr<sz::toml_config>& config, QWidget* parent)
    : QDialog(parent), m_config(config)
    {
        m_menulist = new QListWidget(this);
        m_menulist->addItem("General");
        m_menulist->addItem(tr("Navigation"));
        
        m_pages = new QStackedWidget(this);

        m_general_settings = new general_settings_form(config, this);
        m_navpanel_settings = new navigation_panel_settings_form(config, this);
        m_view_settings = new view_settings_form(config, this);
        m_pages->addWidget(m_general_settings);
        m_pages->addWidget(m_navpanel_settings);
        m_pages->addWidget(m_view_settings);

        // buttons
        m_button_box = new QDialogButtonBox(
            QDialogButtonBox::StandardButton::Cancel | QDialogButtonBox::StandardButton::Ok,
            Qt::Orientation::Horizontal,
            this
        );

        m_apply_button = m_button_box->addButton(QDialogButtonBox::StandardButton::Apply);
        
        m_hsplit = new QSplitter;
        m_hsplit->addWidget(m_menulist);
        m_hsplit->addWidget(m_pages);
        
        m_vbox = new QVBoxLayout(this);
        m_vbox->addWidget(m_hsplit);
        m_vbox->addWidget(m_button_box);

        connect(m_menulist, &QListWidget::itemClicked, this, &settings_dialog::change_page);
        connect(m_button_box, &QDialogButtonBox::accepted, this, &settings_dialog::apply_settings_and_accept);
        connect(m_button_box, &QDialogButtonBox::rejected, this, &settings_dialog::reject);
        connect(m_button_box, &QDialogButtonBox::clicked, this, &settings_dialog::handle_button_click);
    }

    settings_dialog::~settings_dialog()
    {
    }

    void settings_dialog::change_page(QListWidgetItem* page_name_widget)
    {
        auto text = page_name_widget->text();
        qDebug() << "[settings dialog] change page to: " << text;

        // TODO: we should store enums inside the list items instead of comparing strings
        if (text == "General")
        {
            m_pages->setCurrentWidget(m_general_settings);
        }
        else if (text == "Navigation")
        {
            m_pages->setCurrentWidget(m_navpanel_settings);
        }
    }

    bool settings_dialog::apply_settings()
    {
        if (m_pages->currentWidget() == m_general_settings)
        {
            if (!m_general_settings->can_apply_settings())
            { return false; }
        }
        else if (m_pages->currentWidget() == m_navpanel_settings)
        {   
            if (!m_navpanel_settings->can_apply_settings())
            { return false; }
        }
        qDebug() << "[settings dialog] apply settings";
        emit settings_applied();
        return true;
    }

    void settings_dialog::apply_settings_and_accept()
    {
        if (apply_settings())
        {
            accept();
        }
    }

    void settings_dialog::handle_button_click(QAbstractButton* button)
    {
        if (button == m_apply_button)
        {
            apply_settings();
        }
    }
} // namespace rin
