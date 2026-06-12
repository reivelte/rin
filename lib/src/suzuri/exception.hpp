// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <exception>
#include "types.hpp"
#include "result.hpp"

namespace sz
{
    // common exception base
    class exception : public sz::common_error, public std::exception
    {
        public:
        template <is_int_compatible T>
        exception(T code, std::string_view msg = "", std::source_location loc = std::source_location::current())
        : common_error(code, msg, loc) {}

        virtual ~exception() = default;

        const char* what() const noexcept override 
        { 
            m_what = std::format("{}. {}", location(), message());
            return m_what.c_str();
        }

        private:
        mutable std::string m_what;
    };
}