// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <iostream>
#include "shared_library.hpp"

namespace sz
{
    shared_library::shared_library() :
        m_handle(nullptr), m_ref(), m_path()
    {
    }

    shared_library::shared_library(shared_library&& rhs) :
        m_handle(rhs.m_handle), m_ref(std::move(rhs.m_ref)), m_path(rhs.m_path)
    {
    }

    shared_library::shared_library(const shared_library& rhs) :
        m_handle(rhs.m_handle), m_ref(rhs.m_ref), m_path(rhs.m_path)
    {
    }

    shared_library::shared_library(const std::filesystem::path& path) :
        m_handle(nullptr), m_ref(), m_path(path)
    {
        m_init(path);
    }

    shared_library::~shared_library()
    {
        m_close();
    }

    shared_library& shared_library::operator=(shared_library&& rhs)
    {
        m_close();
        m_handle = rhs.m_handle;
        m_ref = std::move(rhs.m_ref);
        m_path = rhs.m_path;
        return *this;
    }

    shared_library& shared_library::operator=(const shared_library& rhs)
    {
        m_close();
        m_handle = rhs.m_handle;
        m_ref = rhs.m_ref;
        m_path = rhs.m_path;
        return *this;
    }

    // expects the string_view to point to a null-terminated string
    result<void*> shared_library::load_symbol(std::string_view name) const
    {
#ifndef _WIN32
        if (m_handle && name.size())
        {
            char* error = dlerror();

            if (error)
            { return common_error(result_code::Init_Problem, error); }

            void* func = dlsym(m_handle, name.data());

            if (func == NULL)
            { return common_error(result_code::No_Action, dlerror()); }

            return func;
        }
        return common_error(result_code::Interface_Misuse);
#else
        return common_error(result_code::No_Action, "not implemented");
#endif
        
    }

    void shared_library::m_init(const std::filesystem::path& path)
    {
#ifndef _WIN32
        m_handle = dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);

        if (m_handle == NULL)
        {
            // TODO: we dont want to print messages inside constructors
            std::cerr << dlerror() << '\n';
        }
#endif
    }

    void shared_library::m_close()
    {
        if (m_ref <= 0)
        {
#ifndef _WIN32
            if (m_handle != NULL)
            { dlclose(m_handle); }
#endif
        }
    }

} // namespace sz