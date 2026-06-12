// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <utility>
#include <system_error>
#include <string>
#include <string_view>
#include <source_location>
#include <expected>
#include <format>
#include <print>
#include <filesystem>
#include "types.hpp"

namespace sz
{
    enum class result_code : int
    {
        Uninitialized = -1, Success = 0, 
        Unspecified_Runtime_Error, File_Not_Found, Exception_Occurred, 

        No_Action, Redundant_Action,
        Init_Problem, Incomplete_Result,

        Invalid_Argument, Index_Out_Of_Bounds, Unknown_Key, 
        Interface_Misuse, Logic_Error
    };

    struct indexed_result_code
    {
        size_t index{0};
        result_code code{result_code::Uninitialized};

        explicit constexpr operator size_t() const { return index; }
        explicit constexpr operator result_code() const { return code; }
        
        auto operator<=>(const indexed_result_code&) const = default;
    };

    class common_error 
    {
        public:
        common_error() = default;

        template <is_int_compatible T>
        constexpr common_error(T code, std::string_view msg = "", std::source_location loc = std::source_location::current()) 
        : m_error_code([&]
            {
                if constexpr (std::is_enum_v<T>) 
                { return std::to_underlying(code); }
                else 
                { return static_cast<int>(code); }
            }()),

        m_error_type(type_name<T>()), m_location(loc), m_msg(std::string(msg)) {}

        explicit common_error(std::error_code&& err_code, std::source_location loc = std::source_location::current())
        : m_error_code(err_code.value()), 
        m_error_type(type_name<std::error_code>()), m_location(loc), m_msg(std::string(err_code.category().name()) + ": " + err_code.message()) {}

        common_error(const std::error_code& err_code, std::source_location loc = std::source_location::current())
        : m_error_code(err_code.value()), 
        m_error_type(type_name<std::error_code>()), m_location(loc), m_msg(std::string(err_code.category().name()) + ": " + err_code.message()) {}

        common_error(common_error&& rhs) 
        : m_error_code(rhs.m_error_code), 
        m_error_type(rhs.m_error_type), m_location(std::move(rhs.m_location)), m_msg(std::move(rhs.m_msg)) {}

        common_error(const common_error& rhs) = default;
        ~common_error() = default;

        inline int code() const noexcept { return m_error_code; }

        inline std::string location() const 
        {
            auto errtype = m_error_type;
            auto errcode = m_error_code;
            auto path = std::filesystem::path(m_location.file_name()).filename().string();
            auto line = m_location.line();
            auto col = m_location.column();
            return std::format("{}({}) in {}({},{})", errtype, errcode, path, line, col);
        }
        
        inline std::string_view original_type() const noexcept { return m_error_type; }

        inline std::string_view message() const noexcept { return m_msg; }
        inline void set_message(const std::string& what) { m_msg = what; }

        inline void set_location(std::source_location loc) noexcept { m_location = loc; }

        protected:
        int m_error_code;
        std::string_view m_error_type; // substring of __PRETTY_FUNCTION__ in gcc/clang
        std::source_location m_location;
        std::string m_msg;
    };
} // namespace sz

template <>
struct std::formatter<sz::common_error> : std::formatter<std::string> 
{
    auto format(const sz::common_error& e, auto& ctx) const 
    {
        if (e.message().size())
            return std::formatter<std::string>::format(std::format("{}: {}", e.location(), e.message()), ctx);
        else
            return std::formatter<std::string>::format(std::format("{}", e.location()), ctx);
    }
};

namespace sz
{
    template <typename T>
    class result : public std::expected<T, sz::common_error> 
    {
        using base = std::expected<T, sz::common_error>;
        using base::base;

        public:
        result(sz::common_error err_code) :base(std::unexpected(std::move(err_code))) {}
    };

    // TODO*: change out println() for a proper log function
    template <typename T> requires (is_result<T> || std::is_same_v<T, sz::common_error>)
    inline int check_error(const T& r, bool print_message = true, bool use_loc_from_callsite = true, std::source_location loc = std::source_location::current())
    {
        const common_error* e = nullptr;
        if constexpr (is_result<T>)
        {
            if (r)
            { return 0; }

            e = &r.error();
        }
        else
        { e = &r; }
        
        if (use_loc_from_callsite)
        {
            common_error temp = *e;
            temp.set_location(loc);
            
            if (print_message)
            { std::println("{}", temp); }

            return e->code();
        }
        
        if (print_message)
        { std::println("{}", *e); }

        return e->code();
    }

}