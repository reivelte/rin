// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <QtCore/QList>
#include <QtCore/QString>

namespace rin
{
    class history_manager
    {
        public:
        history_manager();

        inline void clear()                                     { m_items.clear(); m_pos = -1; }

        inline const QString& item_at(qsizetype index) const    { return m_items.at(index); }
        inline const QString& current() const                   { return m_items.at(m_pos); }
        inline qsizetype current_position() const               { return m_pos; }

        void step_forward_with(const QString& item);
        bool step_forward();
        bool step_back();
        bool set_position(qsizetype position);

        private:
        QList<QString> m_items;
        qsizetype m_pos;

    };
    
} // namespace rin
