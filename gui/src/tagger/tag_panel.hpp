// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <vector>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QVBoxLayout>
#include "widgets/panel.hpp"
#include "widgets/tag_listing.hpp"
#include "model/entity.hpp"
#include "model/entitymodel.hpp"

namespace rin
{
    class tag_panel : public ui_panel
    {
        Q_OBJECT

        public:
        tag_panel(QWidget* parent, entity_model* model);
        ~tag_panel();

        signals:
        void tags_submitted(const std::vector<reflexive_entity>& tags);

        public slots:
        void read_or_submit_input();

        private:
        entity_model* m_model;
        tag_listing* m_tag_list;
        QLineEdit* m_lineedit;
        QVBoxLayout* m_vbox;

    };
    
    
} // namespace rin
