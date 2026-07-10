// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "header.hpp"

namespace rin
{
    entity_view_header::entity_view_header(QWidget* parent) :
    QHeaderView(Qt::Horizontal, parent)
    {
    }

    entity_view_header::~entity_view_header()
    {
    }

} // namespace rin
