// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <mutex>
#include <sz_export.hpp>

namespace sz
{
    template <typename T>
    class SZ_API guarded_ptr
    {
        public:
        guarded_ptr() = delete;
        guarded_ptr(T& obj, std::mutex& mutex);
        guarded_ptr(const guarded_ptr&) = delete;
        guarded_ptr(guarded_ptr&& rhs);
        ~guarded_ptr();
        void operator=(const guarded_ptr&) = delete;
        inline T& operator*();
        inline T* operator->() const;
        explicit inline operator bool() const;
        
        inline bool is_locked() const;

        private:
        T* m_data;
        std::mutex* m_mutex;
        bool m_locked;
    };

    template <typename T>
    inline guarded_ptr<T>::guarded_ptr(T& obj, std::mutex& mutex) :
        m_data(&obj), m_mutex(&mutex)
    {
        m_locked = m_mutex->try_lock();
    }

    template <typename T>
    inline guarded_ptr<T>::guarded_ptr(guarded_ptr&& rhs) :
        m_data(std::move(rhs.m_data)), m_mutex(std::move(rhs.m_mutex)),
        m_locked(rhs.is_locked())
    {
    }

    template <typename T>
    inline guarded_ptr<T>::~guarded_ptr()
    {
        if (m_locked)
        {
            m_mutex->unlock();
            m_locked = false;
        }
    }

    template <typename T>
    inline guarded_ptr<T>::operator bool() const
    {
        return is_locked();
    }

    template <typename T>
    inline T& guarded_ptr<T>::operator*()
    {
        return *m_data;
    }

    template <typename T>
    inline T* guarded_ptr<T>::operator->() const
    {
        return m_data;
    }

    template <typename T>
    inline bool guarded_ptr<T>::is_locked() const
    {
        return m_locked;
    }

} // namespace sz
