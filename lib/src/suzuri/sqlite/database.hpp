// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <unordered_map>
#include <filesystem>
#include <string>
#include <sz_export.hpp>
#include "common.hpp"
#include "query.hpp"

struct sqlite3;

namespace sz::sqlite 
{
    class SZ_API database 
    {
        public:
        database();
        explicit database(const std::filesystem::path& path);
        database(database&& rhs);
        database(const database&) = delete;
        ~database();
        database& operator=(database&& rhs);
        database& operator=(const database&) = delete;

        void connect(const std::filesystem::path& path);
        
        inline void clear_cache() noexcept { m_cache.clear(); }
        
        /* getters */
        status_code status() const noexcept;
        const std::unordered_map<std::string, database_query>& cache() const noexcept;
        std::string error_message(const std::string& sql_for_annotation = "") const;

        /* make queries against the database */
        template <typename... Binds, std::size_t N = sizeof...(Binds)> requires ((is_bindable<Binds> && ...) || ((std::ranges::range<Binds> && ...) && N == 1))
        [[nodiscard]] database_query& query(std::string sql, Binds&&... binds);
        
        template <typename... Binds, std::size_t N = sizeof...(Binds)> requires ((is_bindable<Binds> && ...) || ((std::ranges::range<Binds> && ...) && N == 1))
        [[nodiscard]] database_query make_query(std::string_view sql, Binds&&... binds);

        /* execute commands on the database */
        template <is_bindable... Binds> status_code cmd(std::string_view sql, Binds&&... binds);

        private:
        void m_open_connection(const std::filesystem::path& path);
        void m_close_connection();

        template <is_bindable... Binds> constexpr void m_make_binds(database_query& q, Binds&&... binds);
        template <is_bindable T> inline void m_make_binds(database_query& q, const std::vector<T>& binds);

        private:
        sqlite3* m_handle;
        status_code m_status;
        std::unordered_map<std::string, database_query> m_cache;
    };

    template <typename... Binds, std::size_t N> requires ((is_bindable<Binds> && ...) || ((std::ranges::range<Binds> && ...) && N == 1))
    [[nodiscard]] inline database_query& database::query(std::string sql, Binds&&... binds)
    {
        auto it = m_cache.find(sql);
        if (it == m_cache.end())
        {
            m_cache.emplace(sql, database_query(m_handle, sql));
            it = m_cache.find(sql);
        }

        using enum status_code;
        if (auto status = it->second.status(); !(status == OK || status == Row || status == Done))
        { return it->second; }

        if constexpr (N == 1 && is_vector<std::tuple_element_t<0, std::tuple<Binds...>>>)
        {
            using VectorType = typename std::decay_t<decltype((std::forward<Binds>(binds), ...))>::value_type;
            static_assert(is_bindable<VectorType>, "Array of values must all be bindable or of sqlite::variant type");
            const std::vector<VectorType>& vec = std::get<0>(std::tuple<Binds...>(std::forward<Binds>(binds)...));
            if (!it->second.has_same_binds(vec))
            {
                it->second.reset_all();
                m_make_binds(it->second, vec);
                it->second.size(); // get the size so vector allocations made in query.rows/xrows() are efficient
            }
        }
        else 
        {
            if (!it->second.has_same_binds(std::forward<Binds>(binds)...))
            {
                it->second.reset_all();
                m_make_binds(it->second, std::forward<Binds>(binds)...);
                it->second.size();
            }
        }

        if (it->second.status() == Done) // we are returning a cached query already in the Done state
        { it->second.reset_stmt(); }

        return it->second;
    }

    template <typename... Binds, std::size_t N> requires ((is_bindable<Binds> && ...) || ((std::ranges::range<Binds> && ...) && N == 1))
    [[nodiscard]] inline database_query database::make_query(std::string_view sql, Binds&&... binds)
    {
        database_query q(m_handle, sql);
        if constexpr (N == 1 && is_vector<std::tuple_element_t<0, std::tuple<Binds...>>>)
        {
            using VectorType = typename std::decay_t<decltype((std::forward<Binds>(binds), ...))>::value_type;
            static_assert(is_bindable<VectorType>, "Array of values must all be bindable or of sqlite::variant type");
            const std::vector<VectorType>& vec = std::get<0>(std::tuple<Binds...>(std::forward<Binds>(binds)...));
            m_make_binds(q, vec);
        }
        else
        { m_make_binds(q, std::forward<Binds>(binds)...); }
        return q;
    }

    template <is_bindable... Binds>
    inline status_code database::cmd(std::string_view sql, Binds&&... binds)
    {
        database_query q(m_handle, sql);
        m_make_binds(q, std::forward<Binds>(binds)...);
        
        for (q.step(); q == status_code::Row; ++q) 
        {}
        return status_code{q};
    }

    template <is_bindable... Binds>
    inline constexpr void database::m_make_binds(database_query& q, Binds&&... binds)
    {
        if constexpr(sizeof...(Binds))
        {
            [&] <std::size_t... I> (std::index_sequence<I...>) constexpr
            {
                (q.bind(binds, static_cast<int>(I)), ...);
            }(std::index_sequence_for<Binds...>{});
        }
    }

    template <is_bindable T>
    inline void database::m_make_binds(database_query& q, const std::vector<T>& binds)
    {
        for (size_t i = 0; i < binds.size(); i++)
        { q.bind(binds[i], static_cast<int>(i)); }
    }

} // namespace sz::sqlite

// sz::sqlite::database& operator<<(sz::sqlite::database& lhs, sz::sqlite::database_query& rhs);