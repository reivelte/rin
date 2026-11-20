// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <memory>
#include <sz_export.hpp>
#include "reference_counter.hpp"

namespace sz
{
    namespace utility
    {
        template <typename T>
        using shared_explicit = std::shared_ptr<T>;

        template <typename T>
        class SZ_API shared_implicit
        {
            public:
            using Self = shared_implicit<T>;

            shared_implicit() noexcept          : m_ref(), m_ptr(nullptr) {}
            explicit shared_implicit(T* ptr)    : m_ref(), m_ptr(ptr) {}
            shared_implicit(Self&& other)       : m_ref(std::move(other.m_ref)), m_ptr(std::move(other.m_ptr)) {}
            shared_implicit(const Self& other)  : m_ref(other.m_ref), m_ptr(other.m_ptr) {}
            ~shared_implicit();

            Self& operator=(Self&& rhs)
            {
                m_detach_and_replace(std::move(rhs.m_ptr));
                m_ref = std::move(rhs.m_ref);
            }
            Self& operator=(const Self&) = default;
            
            T& operator*()
            {
                return *data();
            }
            const T& operator*() const { return *data(); }

            T* operator->() { return data(); }
            const T* operator->() const noexcept { return data(); }
            operator T*() { return data(); }
            operator const T*() const noexcept { return data(); }

            void reset(T* ptr) noexcept;

            T* data();
            const T* data() const noexcept;
            T* get();
            const T* get() const noexcept;

            private:
            inline T* m_clone();
            bool m_is_referenced() const noexcept;
            bool m_is_only_reference() const noexcept;
            void m_detach();
            void m_detach_and_replace(T* ptr = nullptr);
            void m_delete_ptr();
            
            private:
            mutable reference_counter m_ref;
            T* m_ptr;
        };

        template <typename T>
        inline shared_implicit<T>::~shared_implicit()
        {
            if (m_is_only_reference()) {
                m_delete_ptr();
            }
        }

        template <typename T>
        inline void shared_implicit<T>::reset(T *ptr) noexcept
        {
            if (ptr == nullptr)
            {
                if (m_is_only_reference()) {
                    m_delete_ptr();
                }
                m_ref = reference_counter();
            }
            else {
                m_detach_and_replace(ptr);
            }
        }

        template <typename T>
        inline T* shared_implicit<T>::data()
        {
            m_detach();
            return m_ptr;
        }

        template <typename T>
        inline const T* shared_implicit<T>::data() const noexcept
        {
            return m_ptr;
        }

        template <typename T>
        inline T* shared_implicit<T>::get()
        {
            return data();
        }

        template <typename T>
        inline const T* shared_implicit<T>::get() const noexcept
        {
            return data();
        }

        template <typename T>
        inline T* shared_implicit<T>::m_clone()
        {
            return new T(*m_ptr);
        }

        template <typename T>
        inline bool shared_implicit<T>::m_is_referenced() const noexcept
        {
            return m_ref > 1;
        }

        template <typename T>
        inline bool shared_implicit<T>::m_is_only_reference() const noexcept
        {
            return m_ref == 1;
        }

        template <typename T>
        inline void shared_implicit<T>::m_detach()
        {
            if (m_is_referenced())
            {
                m_ref = reference_counter();
                m_ptr = m_clone();
            }
        }

        template <typename T>
        inline void shared_implicit<T>::m_detach_and_replace(T* ptr)
        {
            T* x = ptr == nullptr ? m_clone() : ptr;
            
            if (m_is_only_reference()) {
                m_delete_ptr();
            }
            m_ref = reference_counter();
            m_ptr = x;
        }

        template <typename T>
        inline void shared_implicit<T>::m_delete_ptr()
        {
            if (m_ptr)
            {
                delete m_ptr;
                m_ptr = nullptr;
            }
        }

    } // namepsace utility

} // namespace sz
