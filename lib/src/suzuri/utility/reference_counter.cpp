// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "reference_counter.hpp"

namespace sz
{
    reference_counter::reference_counter() :
        m_count(nullptr)
    {
        reset();
    }

    reference_counter::reference_counter(reference_counter&& rhs) :
        m_count(nullptr)
    {
        m_decrement_and_replace(rhs.m_count);
        rhs.m_count = nullptr;
    }

    reference_counter::reference_counter(const reference_counter& rhs) :
        m_count(nullptr)
    {
        m_decrement_and_replace(rhs.m_count);
        m_increment();
    }

    reference_counter::~reference_counter()
    {
        m_decrement_or_delete();
    }

    reference_counter& reference_counter::operator=(reference_counter&& rhs)
    {
        m_decrement_and_replace(rhs.m_count);
        rhs.m_count = nullptr;
        return *this;
    }

    reference_counter& reference_counter::operator=(const reference_counter& rhs)
    {
        m_decrement_and_replace(rhs.m_count);
        m_increment();
        return *this;
    }

    int reference_counter::operator*() const
    {
        return m_count->load();
    }

    void reference_counter::reset()
    {
        m_decrement_or_delete();
        m_count = new std::atomic<int>;
    }

    void reference_counter::m_increment()
    {
        ++(*m_count);
    }

    void reference_counter::m_decrement()
    {
        --(*m_count);
    }

    void reference_counter::m_decrement_or_delete()
    {
        if (m_count)
        {
            if (m_count->load() > 0)
            { m_decrement(); }
            else
            { delete m_count; }
        }
    }

    void reference_counter::m_decrement_and_replace(std::atomic<int>* ptr)
    {
        if (m_count == ptr)
        { return; }

        m_decrement_or_delete();
        m_count = ptr;
    }

    reference_counter::operator int() const
    {
        return m_count->load();
    }

} // namespace sz
