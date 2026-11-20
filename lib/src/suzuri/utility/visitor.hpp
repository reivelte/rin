// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <tuple>
#include <type_traits>
#include <variant>
#include <utility>
#include <vector>

namespace sz
{
    namespace utility
    {
        // referenced from: https://www.youtube.com/watch?v=EsUmnLgz8QY
        template <typename... Base>
        struct basic_visitor : Base...
        {
            using Base::operator()...;
        };
        template <typename... T> basic_visitor(T...) -> basic_visitor<T...>;

        template <typename... Funcs, typename... Types>
        inline void type_conditional_call(basic_visitor<Funcs...> visitor, Types&&... values)
        {
            using Tuple_Of_Types = decltype(std::tuple<std::remove_cvref_t<Types>...>());
            using T = std::tuple_element_t<0, Tuple_Of_Types>;
            using Ts = typename std::decay_t<decltype((std::forward<Types>(values), ...))>;
            constexpr std::size_t N = sizeof...(Types);

            constexpr bool all_same_type = [&] <std::size_t... Idx> (std::index_sequence<Idx...>) constexpr -> bool
            {
                return ((std::same_as<T, std::tuple_element_t<Idx, Tuple_Of_Types>>) && ...);
            }(std::index_sequence_for<Types...>{});

            if constexpr (N > 1 && all_same_type)
            {
                std::vector<T> x{values...};
                std::variant<std::vector<T>> v;
                v = x;
                std::visit(visitor, v);
            }
            else
            {
                const std::array<std::variant<Ts>, sizeof...(Types)> x{std::variant<Ts>(values)...};
                for (const auto& v : x)
                { std::visit(visitor, v); }
            }
        }
    }
    
} // namespace sz
