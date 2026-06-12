// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "historymanager.hpp"

namespace rin
{
    history_manager::history_manager()
    : m_items(), m_pos(-1)
    {
        
    }

    void history_manager::step_forward_with(const QString& item)
    {
        ++m_pos;
        m_items.insert(m_pos, item);
        
        if (qsizetype start = m_pos + 1; start < m_items.size())
        { m_items.remove(start, m_items.size() - start); }
    }

    bool history_manager::step_forward()
    {
        if (m_pos + 1 < m_items.size())
        { 
            ++m_pos;
            return true;
        }
        return false;
    }

    bool history_manager::step_back()
    {
        if (m_pos - 1 > -1)
        { 
            --m_pos;
            return true;
        }
        return false;
    }

    bool history_manager::set_position(qsizetype position)
    {
        if (position >= 0 && position < m_items.size())
        { 
            m_pos = position; 
            return true;
        }
        return false;
    }

} // namespace rin
