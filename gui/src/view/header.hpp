// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <QtWidgets/QHeaderView>

namespace rin
{
    class entity_view_header : public QHeaderView
    {
        Q_OBJECT

        public:
        explicit entity_view_header(QWidget* parent);
        ~entity_view_header();

    };
    
} // namespace rin
