// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <sqlite3.h>
#include "query.hpp"

namespace sz::sqlite
{
    /* public */
    database_query::database_query() 
    : m_binds(), m_sql(), m_conn(nullptr), m_handle(nullptr), m_pos(0), m_size(0), m_status(status_code::Uninitialized) 
    {

    }
    
    database_query::database_query(sqlite3* conn, std::string_view sql) 
    : m_binds(), m_sql(sql), m_conn(conn), m_handle(nullptr), m_pos(0), m_size(0), m_status(status_code::Uninitialized) 
    {
        // if (!query_str.empty())
        // { m_status = status_code{sqlite3_prepare_v2(conn, query_str.data(), static_cast<int>(query_str.size()), &m_handle, nullptr)}; }
        m_init();
    }

    database_query::database_query(sqlite3* conn, std::string_view sql, const std::vector<variant>& binds)
    : m_binds(binds), m_sql(sql), m_conn(conn), m_handle(nullptr), m_pos(0), m_size(0), m_status(status_code::Uninitialized) 
    {
        m_init();
        m_rebind();
    }

    database_query::database_query(database_query&& rhs) 
    : m_binds(std::move(rhs.m_binds)), m_sql(rhs.m_sql), m_conn(rhs.m_conn), m_handle(rhs.m_handle), m_pos(rhs.m_pos), m_size(rhs.m_size), m_status(rhs.m_status)
    {
        rhs.m_handle = nullptr;
        rhs.m_pos = 0;
        rhs.m_size = 0;
        rhs.m_status = status_code::Uninitialized;
    }

    database_query::~database_query() 
    {
        m_try_finalize();
    }

    database_query& database_query::operator++()
    {
        if (m_handle)
        {
            if (m_status != status_code::Done) 
            {
                m_status = status_code{sqlite3_step(m_handle)}; 
                ++m_pos;
            }
        }
        return *this;
    }

    status_code database_query::step() 
    {
        ++*this;
        return m_status;
    }

    status_code database_query::restep()
    {
        if (++*this == status_code::Done)
        {
            reset_stmt();
            ++*this;
        }
        return m_status;
    }

    database_query::iterator database_query::begin()
    {
        iterator it(this);
        if (m_engage())
        {
            reset_stmt();
            ++it;
        }
        return it;
    }

    std::vector<variant> database_query::operator*()
    {
        if (m_engage())
        {
            return row();
        }
        return std::vector<variant>();
    }

    std::vector<variant> database_query::row()
    {
        std::vector<variant> row;
        if (m_engage())
        {
            int col_count = sqlite3_data_count(m_handle);
            row.reserve(col_count);
            
            for (int i = 0; i < col_count; ++i)
            { row.emplace_back(column(i)); }
        }
        return row;
    }

    size_t database_query::size() const
    {
        m_get_size();
        return m_size;
    }

    variant database_query::column(int column_index) const
    {
        if (m_status == status_code::Row)
        {
            switch(sqlite3_column_type(m_handle, column_index))
            {
            case SQLITE_INTEGER: 
                return static_cast<int64_t>(sqlite3_column_int64(m_handle, column_index));

            case SQLITE_FLOAT:
                return static_cast<double>(sqlite3_column_double(m_handle, column_index));

            default:
                auto data = std::bit_cast<const char*>(sqlite3_column_text(m_handle, column_index));
                return data ? data : "";
            }
        }
        return variant();
    }

    void database_query::set_sql(std::string_view sql)
    {
        m_try_finalize();
        m_sql = std::string(sql);
        m_init();
    }

    void database_query::set_sql(std::string_view sql, const std::vector<variant>& binds)
    {
        set_sql(sql);
        bind(binds);
    }

    bool database_query::has_same_binds(const std::vector<variant> &binds) const
    {
        if (binds.size() != m_binds.size())
        { return false; }
        else 
        { 
            for (size_t i = 0; i < binds.size(); ++i)
            {
                if (m_binds[i] != binds[i])
                { return false; }
            }
        }
        return true;
    }

    status_code database_query::bind(const variant& val, int bind_index)
    {
        if (std::holds_alternative<std::string>(val)) { 
            return bind(std::get<std::string>(val), bind_index); 
        }
        else if (std::holds_alternative<int64_t>(val)) { 
            return bind(std::get<int64_t>(val), bind_index); 
        }
        
        return bind(std::get<double>(val), bind_index);
    }

    status_code database_query::bind(const std::vector<variant>& binds)
    {
        using enum status_code;
        if (m_status != status_code::Uninitialized)
        { reset_bindings(); }
        for (size_t i = 0; i < binds.size(); ++i)
        {
            auto s = bind(binds[i], static_cast<int>(i));
            if (s != OK)
            { return s; }
        }
        return OK;
    }

    void database_query::rebind()
    {
        if (m_binds.size())
        { m_rebind(); }
    }

    database_query &database_query::operator=(database_query&& rhs)
    {
        m_reset(rhs.m_handle, rhs.m_status);
        m_binds = std::move(rhs.m_binds);
        m_sql = rhs.m_sql;
        m_conn = rhs.m_conn;
        m_pos = rhs.m_pos;
        m_size = rhs.m_size;

        rhs.m_pos = 0;
        rhs.m_size = 0;
        rhs.m_handle = nullptr;
        rhs.m_status = status_code::Uninitialized;
        return *this;
    }

    // should we set m_status here? sqlite3_reset will return OK even if m_handle is nullptr
    void database_query::reset_stmt() 
    { 
        m_status = status_code{sqlite3_reset(m_handle)}; 
        m_pos = 0;
    }
    
    void database_query::reset_bindings() 
    { 
        sqlite3_clear_bindings(m_handle);
        m_pos = 0;
        m_size = 0;
        m_binds.clear();
    }

    void database_query::reset_all()
    {
        reset_stmt();
        reset_bindings();
    }

    void database_query::clear()
    {
        m_try_finalize();
        m_binds.clear();
        m_sql.clear();
        m_conn = nullptr;
        m_handle = nullptr;
        m_pos = 0;
        m_size = 0;
        m_status = status_code::Uninitialized;
    }

    /* private */

    status_code database_query::m_bind_double(int index, double v)
    {
        return status_code(sqlite3_bind_double(m_handle, index, v));
    }

    status_code database_query::m_bind_int(int index, int v)
    {
        return status_code(sqlite3_bind_int(m_handle, index, v));
    }

    status_code database_query::m_bind_int64(int index, int64_t v)
    {
        return status_code(sqlite3_bind_int64(m_handle, index, v));
    }

    status_code database_query::m_bind_text(int index, const char* v)
    {
        return status_code(sqlite3_bind_text(m_handle, index, v, -1, SQLITE_TRANSIENT));
    }

    void database_query::m_finalize()
    {
        m_pos = 0;
        m_size = 0;
        status_code finalize_result = status_code{sqlite3_finalize(m_handle)};
        if (finalize_result == status_code::OK)
        { m_status = status_code::Uninitialized; }
        else
        { m_status = finalize_result; }

        m_handle = nullptr;
    }

    void database_query::m_try_finalize() 
    {
        if (m_handle)
        { m_finalize(); }
    }

    void database_query::m_reset(sqlite3_stmt* stmt, status_code s) 
    {
        m_try_finalize();
        m_handle = stmt;
        m_status = s;
    }

    void database_query::m_get_size() const
    {
        // using enum status_code;
        if (!m_size)
        {
            // size_t prev_pos = m_pos;
            // size_t size = 0;
            // if (m_engage())
            // {
            //     reset_stmt();
            //     while(m_status == Row)
            //     {
            //         ++size;
            //         ++*this;
            //     }
            //     reset_stmt();
            //     for (size_t i = 0; i < prev_pos; ++i)
            //     { ++*this; }
            // }
            // m_size = size;
            // TODO: make a sql one-shot function
            std::string sql = std::format("SELECT COUNT(1) FROM ({})", m_sql);
            sqlite3_stmt* stmt = nullptr;
            sqlite3_prepare_v2(m_conn, sql.c_str(), static_cast<int>(sql.size()), &stmt, nullptr); 
            if (m_binds.size())
            {
                for (size_t i = 0; i < m_binds.size(); ++i)
                {
                    int col = static_cast<int>(i) + 1;
                    if (std::holds_alternative<std::string>(m_binds[i])) { 
                        sqlite3_bind_text(stmt, col, std::get<std::string>(m_binds[i]).c_str(), -1, SQLITE_TRANSIENT);
                    }
                    else if (std::holds_alternative<int64_t>(m_binds[i])) { 
                        sqlite3_bind_int64(stmt, col, std::get<int64_t>(m_binds[i])); 
                    }
                    else
                    { sqlite3_bind_double(stmt, col, std::get<double>(m_binds[i])); }
                }
            }
            // [[maybe_unused]] auto result = status_code{sqlite3_step(stmt)};
            sqlite3_step(stmt);
            m_size = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
            sqlite3_finalize(stmt);
        }
    }

    void database_query::m_init()
    {
        m_try_finalize();
        if (!m_sql.empty() && m_conn)
        { 
            m_status = status_code{sqlite3_prepare_v2(m_conn, m_sql.c_str(), static_cast<int>(m_sql.size()), &m_handle, nullptr)}; 
        }
        else
        {
            m_status = status_code::Uninitialized;
        }
    }

    void database_query::m_rebind()
    {
        std::vector<variant> binds = std::move(m_binds); // this keeps assumptions made in bind() accurate, bind() will reappend the values to m_binds later
        m_binds = std::vector<variant>();
        m_binds.reserve(binds.size());
        for (size_t i = 0; i < binds.size(); ++i)
        { bind(binds[i], static_cast<int>(i)); }
    }

    bool database_query::m_engage()
    {
        if (!good())
        {
            if (m_status == status_code::Uninitialized)
            {
                m_init();
                m_rebind();
            }
            else
            {
                // in the done state or error state
                return false;
            }
        }
        if (m_status == status_code::OK)
        {
            ++*this; // move to first row (or done if no rows in result set)
        }
        return m_status == status_code::Row;
    }
} // namespace sz::sqlite