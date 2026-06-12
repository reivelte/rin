// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <sqlite3.h>
#include "database.hpp"

namespace sz::sqlite
{       
    database::database() :
        m_handle(nullptr), m_status(status_code::Uninitialized)
    {
    }

    database::database(const std::filesystem::path& path) :
        m_handle(nullptr), m_status()
    {
        m_open_connection(path);
    }

    database::database(database&& rhs) 
    :m_handle(rhs.m_handle), m_status(rhs.m_status), m_cache(std::move(rhs.m_cache))
    {
        rhs.m_handle = nullptr;
        rhs.m_status = status_code::Uninitialized;
    }

    database::~database() 
    {
        m_close_connection();
    }

    database& database::operator=(database&& rhs) 
    {
        if (rhs.m_handle != m_handle)
        {
            if (m_handle)
            { m_close_connection(); }

            m_handle = rhs.m_handle;
            m_status = rhs.m_status;
            m_cache = std::move(rhs.m_cache);
            rhs.m_handle = nullptr;
            rhs.m_status = status_code::Uninitialized;
        }
        return *this;
    }

    void database::connect(const std::filesystem::path& path)
    {
        m_close_connection();
        m_open_connection(path);
    }

    status_code database::status() const noexcept
    {
        return m_status;
    }

    const std::unordered_map<std::string, database_query>& database::cache() const noexcept
    {
        return m_cache;
    }

    // it is up to the caller to provide the original sql that caused the error to occur
    std::string database::error_message(const std::string& sql_for_annotation) const
    {
        std::string err_msg = sqlite3_errmsg(m_handle);
        if (err_msg.empty())
        { return err_msg; }

        const std::string prefix = "sqlite: ";
        int token_offset = sqlite3_error_offset(m_handle);
        
        if (token_offset == -1 || sql_for_annotation.empty() || std::cmp_greater(token_offset, sql_for_annotation.size()))
        { return prefix + err_msg; }
        
        static constexpr std::string_view where = "here---^"; // 8 bytes fits into a single 64-bit register
        token_offset -= 6;
        
        std::string annotated_input;
        if (token_offset > 0)
        { annotated_input = std::format(" {}\n{}{}", sql_for_annotation, std::string(token_offset, ' '), where); }
        else
        { annotated_input = std::format("{}{}\n{}", std::string(std::abs(token_offset), ' '), sql_for_annotation, where); }

        return std::format("{}{}\n{}", prefix, err_msg, annotated_input);
    }

    void database::m_open_connection(const std::filesystem::path& path)
    {
        const auto path_ = path.string();
        m_status = status_code(sqlite3_open_v2(path_.c_str(), &m_handle, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr));
    }

    void database::m_close_connection()
    { 
        m_cache.clear();
        m_status = status_code{sqlite3_close_v2(m_handle)}; 
    }
}
