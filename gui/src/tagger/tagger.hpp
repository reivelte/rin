// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <vector>
#include <QtWidgets/QDialog>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QDialogButtonBox>
#include <QtGui/QFileSystemModel>
#include "widgets/details_panel.hpp"
#include "widgets/dialogs/sidecar.hpp"
#include "view/view.hpp"
#include "model/entity.hpp"
#include "model/entitymodel.hpp"
#include "tag_panel.hpp"

namespace rin
{
    class tagger : public QDialog
    {
        Q_OBJECT

        public:
        tagger(QWidget* parent, entity_model* model);
        ~tagger();

        signals:
        void about_to_close();

        public slots:
        void choose_folder();
        void choose_items();
        void set_current_item(const QModelIndex& index);
        void submit_job(const std::vector<reflexive_entity>& tags);
        void submit_job(const QString& entity_id, const sz::metadata::parameters& args);
        void handle_query_progress(QString query_string, QList<std::tuple<QString, int>> processed);

        void spawn_sidecar_dialog();
        void load_from_sidecar();

        private:
        using enum entity_attribute_type;
        QList<QModelIndex> m_selected_items;
        sidecar_dialog* m_sidecar_dialog;
        entity_model* m_model;
        details_panel* m_details_panel;
        entity_view* m_view;
        tag_panel* m_tag_panel;
        QSplitter* m_view_splitter; // vertical
        QSplitter* m_horizontal_splitter;
        QDialogButtonBox* m_buttons;
        QVBoxLayout* m_vlayout;
    };
    
    
} // namespace rin
