// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <string>
#include <string_view>
#include <variant>
#include <format>
#include "../types.hpp"

struct sqlite3;
struct sqlite3_stmt;

namespace sz::sqlite 
{
    enum class status_code : int
    {
        Uninitialized = -1, OK = 0, Error, Internal, Perm, Abort, Busy, Locked, No_Mem, Read_Only, Interrupt,
        IO_Error, Corrupt, Not_Found, Full, Cant_Open, Protocol,
        Empty, Schema, Too_Big, Constraint, Mismatch, Misuse, No_LFS, Auth, Format, Range, Not_A_DB, Notice, Warning,
        Row = 100, Done = 101
    };

    class variant : public std::variant<int64_t, double, std::string> 
    {
        public:
        using base = std::variant<int64_t, double, std::string>;
        using base::base;

        explicit variant(std::string_view s) : base(std::string(s)) {}
        explicit variant(uintmax_t v) : base(static_cast<int64_t>(v)) {}

        template <typename T> requires (std::is_enum_v<T>)
        constexpr operator T() const                    { return static_cast<T>(std::get<int64_t>(*this)); }

        constexpr operator int() const                  { return static_cast<int>(std::get<int64_t>(*this)); }
        constexpr explicit operator int64_t() const     { return std::get<int64_t>(*this); }
        constexpr explicit operator uintmax_t() const   { return static_cast<uintmax_t>(std::get<int64_t>(*this)); }
        constexpr explicit operator double() const      { return std::get<double>(*this); }
        constexpr explicit operator float() const       { return static_cast<float>(std::get<double>(*this)); }
        constexpr operator std::string_view() const { return std::string_view(std::get<std::string>(*this)); }
        constexpr operator std::string() const          { return std::get<std::string>(*this); }
    };

    template <typename T>
    concept is_bindable = requires(T t)
    {
        requires is_int_compatible<T> 
        || std::is_convertible_v<T, std::string> 
        || std::constructible_from<std::string, T>
        || std::is_same_v<T, float> 
        || std::is_same_v<T, double>;
        requires !std::is_same_v<T, bool>;
        requires std::constructible_from<variant, T>;
    };
}

template <>
struct std::formatter<sz::sqlite::variant> : std::formatter<std::string> 
{
    auto format(const sz::sqlite::variant& p, auto& ctx) const 
    {
        return std::visit([&](const auto& value) 
        {
            return std::formatter<std::string>::format(std::format("{}", value), ctx);
        }, static_cast<const sz::sqlite::variant::base&>(p));
    }
};