// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <tuple>
#include <sz_export.hpp>
#include "types.hpp"

namespace sz
{
    class SZ_API vector2
    {
        public:
        constexpr vector2() = default;
        constexpr vector2(i32 x, i32 y) : m_x(x), m_y(y) {}
        ~vector2() = default;

        constexpr void set_x(i32 x) noexcept { m_x = x; }
        constexpr void set_y(i32 y) noexcept { m_y = y; }

        constexpr i32 x() const noexcept { return m_x; }
        constexpr i32 y() const noexcept { return m_y; }

        private:
        i32 m_x;
        i32 m_y;
    };
    
} // namespace sz
