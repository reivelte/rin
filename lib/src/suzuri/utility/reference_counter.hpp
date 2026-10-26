// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <atomic>
#include <sz_export.hpp>
#include "../result.hpp"

namespace sz
{
    class SZ_API reference_counter
    {
        public:
        reference_counter();
        reference_counter(reference_counter&& rhs);
        reference_counter(const reference_counter& rhs); // ++
        ~reference_counter(); // --

        reference_counter& operator=(reference_counter&& rhs);
        reference_counter& operator=(const reference_counter& rhs);
        int operator*() const;
        operator int() const;

        void reset();

        private:
        void m_increment();
        void m_decrement();
        void m_decrement_or_delete();
        void m_decrement_and_replace(std::atomic<int>* ptr); // replace this with ptr

        private:
        std::atomic<int>* m_count;
    };
    
} // namespace sz
