// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "tag_panel.hpp"
#include <suzuri/utility/string.hpp>
#include <suzuri/entity/common.hpp>

namespace rin
{
    tag_panel::tag_panel(QWidget* parent, entity_model* model) :
        ui_panel(parent),
        m_model(model), m_lineedit(nullptr), m_vbox(nullptr)
    {
        m_view = new tag_view(this, model, tag_view::viewmode::Block);
        m_lineedit = new QLineEdit(this);

        m_vbox = new QVBoxLayout(this);
        m_vbox->addWidget(m_view);
        m_vbox->addWidget(m_lineedit);

        connect(m_lineedit, &QLineEdit::returnPressed, this, &tag_panel::read_or_submit_input);
    }

    tag_panel::~tag_panel()
    {
    }

    // check for valid tag(s) in the lineedit, and if so, add it to the list
    // otherwise, signal intent to submit to the tagger widget
    void tag_panel::read_or_submit_input()
    {
        if (auto text = m_lineedit->text().toStdString(); text.empty())
        {
            emit tags_submitted(m_view->tags());
            return;
        }
        else
        {
            std::vector<reflexive_entity> tags;
            auto inputs = sz::utility::split(",", text);
            for (auto& input : inputs)
            {
                input = sz::utility::trim(input);
                if (sz::is_valid_tag(input))
                {
                    if (m_model->entity_exists(input))
                    {
                        // get entity from db
                    }
                    else
                    {
                        tags.emplace_back(QString::fromStdString(input), sz::entity_type::Tag);
                    }
                }
            }
            m_lineedit->clear();
            m_view->append_tags(tags);
        }
    }

} // namespace rin
