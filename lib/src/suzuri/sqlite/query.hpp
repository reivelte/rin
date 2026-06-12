// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <utility>
#include <string_view>
#include <vector>
#include <iterator>
#include <sqlite3.h>
#include <sz_export.hpp>
#include "common.hpp"

namespace sz::sqlite 
{
    class SZ_API database_query 
    {
        public:
        class sentinel
        {
            friend constexpr bool operator==(sentinel, sentinel) noexcept { return true; }
            friend constexpr bool operator!=(sentinel, sentinel) noexcept { return false; }
        };

        class iterator
        {
            public:
            using iterator_category = std::input_iterator_tag;
            using value_type = variant;
            using difference_type = int;
            using pointer = const value_type*;
            using reference = const value_type&;

            iterator() = default;
            explicit iterator(database_query* q) { m_query = q; }
            iterator(iterator&&) noexcept = default;
            iterator& operator=(iterator&&) noexcept = default;

            reference operator*() const { return m_data; }
            pointer operator->() const { return &m_data; }
            iterator& operator++() { ++(*m_query); m_data = m_query->get(); return *this; }
            void operator++(int) { ++*this; }

            private:
            friend class database_query;
            iterator(const iterator&) = delete;
            iterator& operator=(const iterator&) = delete;
            bool at_end() const noexcept { return !m_query->good(); }
            friend bool operator==(const iterator& lhs, sentinel) noexcept { return lhs.at_end(); }
            database_query* m_query;
            mutable variant m_data;
        };
        using const_iterator = iterator;
        
        database_query();
        database_query(sqlite3* conn, std::string_view sql);
        database_query(sqlite3* conn, std::string_view sql, const std::vector<variant>& binds);
        database_query(database_query&& rhs);
        database_query(const database_query&) = delete;
        ~database_query();
        
        /* query stepping */
        database_query& operator++();
        status_code step();
        status_code restep();
        inline size_t pos() const noexcept { return m_pos; }
        iterator begin();
        const_iterator cbegin() const { return const_cast<database_query*>(this)->begin(); } // TODO: get rid of const_cast here
        sentinel end() { return {}; }
        sentinel cend() const { return const_cast<database_query*>(this)->end(); }
        
        /* query data getters */
        template <typename T, std::size_t I = 0> T xrow();
        template <typename... Types> std::tuple<Types...> row();
        template <typename... Types> std::vector<std::tuple<Types...>> rows();
        template <typename T, std::size_t I = 0> std::vector<T> xrows();
        template <typename T, std::size_t I = 0> requires (!std::is_same_v<T, variant>) T column() const;
        template <typename T> T get() const { return column<T, 0>(); }

        std::vector<variant> operator*();
        std::vector<variant> row();
        size_t size() const;
        variant column(int column_index) const; // zero-indexed
        inline variant get() const { return column(0); }
        
        /* get and set sql */
        inline explicit operator std::string() const noexcept { return m_sql; }
        inline explicit operator std::string_view() const noexcept { return m_sql; }
        inline std::string_view sql() const noexcept { return m_sql; }
        void set_sql(std::string_view sql);
        void set_sql(std::string_view sql, const std::vector<variant>& binds);
        
        /* status checkers */
        inline bool good() const { return ((m_status == status_code::OK) || (m_status == status_code::Row)); }
        inline bool at_row() const { return m_status == status_code::Row; }
        inline explicit operator bool() const noexcept { return good(); }
        inline operator status_code() const noexcept { return m_status; }
        inline status_code status() const noexcept { return m_status; }
        
        /* binding */
        template <typename... Binds, std::size_t N = sizeof...(Binds)> bool has_same_binds(Binds&&... binds) const;
        template <is_bindable T> bool has_same_binds(const std::vector<T>& binds) const;
        template <is_bindable T> requires (!std::is_same_v<variant, T>) status_code bind(const T& val, int bind_index);

        bool has_same_binds(const std::vector<variant>& binds) const;
        status_code bind(const variant& val, int bind_index);
        status_code bind(const std::vector<variant>& binds);
        void rebind();

        /* resetters */
        database_query& operator=(database_query&& rhs);
        database_query& operator=(const database_query& rhs) = delete;
        void reset_stmt();
        void reset_bindings();
        void reset_all();
        void clear();
        
        private:
        template<typename... Types> std::tuple<Types...> m_get_row(); // unchecked

        status_code m_bind_double(int index, double v);
        status_code m_bind_int(int index, int v);
        status_code m_bind_int64(int index, int64_t v);
        status_code m_bind_text(int index, const char* v);

        void m_finalize();
        void m_try_finalize();
        void m_reset(sqlite3_stmt* stmt, status_code s);

        void m_get_size() const;
        void m_init();
        void m_rebind();
        bool m_engage();

        private:
        std::vector<variant> m_binds;
        std::string m_sql;
        sqlite3* m_conn;
        sqlite3_stmt* m_handle;
        size_t m_pos;
        mutable size_t m_size; // this is only set when the user requests for the size of their query using size()
        status_code m_status;
    };

    template <typename T, std::size_t I>
    inline T database_query::xrow()
    {
        if (m_engage())
        {
            return std::get<I>(m_get_row<T>());
        }
        return T();
    }

    template <typename... Types>
    inline std::tuple<Types...> database_query::row()
    {
        if (m_engage())
        {
            return m_get_row<Types...>();
        }
        return std::tuple<Types...>();
    }

    template <typename... Types>
    inline std::vector<std::tuple<Types...>> database_query::rows()
    {
        std::vector<std::tuple<Types...>> v;
        if (m_engage())
        {
            reset_stmt();
            
            if (m_size)
            { v.reserve(m_size); }
            
            while (++*this)
            { v.emplace_back(m_get_row<Types...>()); }
        }
        return v;
    }

    template <typename T, std::size_t I>
    inline std::vector<T> database_query::xrows()
    {
        std::vector<T> v;
        if (m_engage())
        {
            reset_stmt();

            if (m_size)
            { v.reserve(m_size); }

            while (++*this)
            { v.emplace_back(column<T, I>()); }
        }
        return v;
    }

    template <typename T, std::size_t I> requires (!std::is_same_v<T, variant>)
    inline T database_query::column() const
    {
        [[maybe_unused]] int col_idx = static_cast<int>(I);
        if (m_status == status_code::Row)
        {
            if constexpr (std::is_same_v<T, std::string> || 
                std::is_convertible_v<T, std::string> || 
                std::constructible_from<std::string, T> ||
                std::constructible_from<T, const char*> || std::constructible_from<T, std::string>)
            {
                auto data = std::bit_cast<const char*>(sqlite3_column_text(m_handle, col_idx));
                return static_cast<T>(data ? data : "");
            }
            else if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>)
            {
                return static_cast<T>(sqlite3_column_double(m_handle, col_idx));
            }
            else if constexpr (is_int_compatible<T>)
            {
                return static_cast<T>(sqlite3_column_int64(m_handle, col_idx));
            }
        }
        return T();
    }

    template <typename... Binds, std::size_t N>
    inline bool database_query::has_same_binds(Binds&&... binds) const
    {
        if constexpr (N)
        {
            if (m_binds.size() != N)
            { return false; }
            else 
            {
                return [&]<std::size_t... I>(std::index_sequence<I...>) constexpr -> bool
                {
                    std::array<variant, N> incoming{variant(binds)...};
                    return ((incoming[I] == m_binds[I]) && ...);
                }(std::index_sequence_for<Binds...>{});
            }
        }

        if (m_binds.size())
        { return false; }

        return true;
    }

    template <is_bindable T>
    inline bool database_query::has_same_binds(const std::vector<T>& binds) const
    {
        if (binds.size() != m_binds.size())
        { return false; }
        else
        {
            if (!std::holds_alternative<T>(m_binds[0]))
            { return false; }
            for (size_t i = 0; i < binds.size(); ++i)
            {
                if (std::get<T>(m_binds[i]) != binds[i])
                { return false; }
            }   
        }
        return true;
    }
    
    template <is_bindable T> requires (!std::is_same_v<variant, T>)
    inline status_code database_query::bind(const T& val, int bind_index)
    {
        ++bind_index; // assume indexes are 0-based and convert to sqlite's expected 1-based index
            
        if (!m_handle)
        { return status_code::Uninitialized; }
        else if (m_status == status_code::Row || m_status == status_code::Done)
        { reset_all(); }

        if (m_status != status_code::OK)
        { return m_status; }

        status_code bind_status = status_code::Uninitialized;

        if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>)
        { bind_status = m_bind_double(bind_index, static_cast<double>(val));  }
        else if constexpr (is_int_compatible<T>)
        {
            if constexpr (std::is_enum_v<T>)
            { bind_status = m_bind_int(bind_index, std::to_underlying(val)); }
            else
            { bind_status = m_bind_int64(bind_index, val); }
        }
        else if constexpr (std::is_convertible_v<T, std::string>)
        { bind_status = m_bind_text(bind_index, static_cast<std::string>(val).c_str()); }
        else
        { return bind_status; }
        
        if constexpr (std::is_enum_v<T>)
        { m_binds.emplace_back(std::to_underlying(val)); }
        else
        { m_binds.emplace_back(val); }

        return bind_status;
    }

    /* private member functions */

    template <typename... Types>
    inline std::tuple<Types...> database_query::m_get_row()
    {
        return [&] <std::size_t... I> (std::index_sequence<I...>) constexpr -> std::tuple<Types...>
        {
            return std::make_tuple( (column<Types, I>())... );
        }(std::index_sequence_for<Types...>{});
    }

} // namespace sz::sqlite