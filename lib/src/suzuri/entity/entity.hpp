// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <string>
#include <unordered_map>
#include <typeindex>
#include <cassert>
#include <sz_export.hpp>
#include "../exception.hpp"
#include "common.hpp"
#include "registry.hpp"

namespace sz
{
    class SZ_API entity
    {
        public:
        constexpr entity();
        constexpr ~entity() = default;
        constexpr entity(entity&&) = default;
        constexpr entity(const entity&) = default;

        constexpr entity& operator=(entity&&) = default;
        constexpr entity& operator=(const entity&) = default;
        constexpr operator size_t() const noexcept;
        constexpr explicit operator entity_type() const noexcept;
        explicit operator bool() const noexcept;
        
        constexpr entity_index index() const noexcept;
        constexpr entity_type type() const noexcept;
        bool valid() const noexcept;
        
        template <typename T> explicit operator T&();
        template <typename T, typename... Args> void emplace(Args&&... args);
        template <typename T> T& get();
        template <typename T> const T& get() const;
        template <typename T> int64_t index() const noexcept;


        protected:
        friend class entity_registry;
        constexpr entity(entity_registry* reg, entity_index index, entity_type type);

        private:
        entity_registry* m_registry;
        entity_index m_index;
        entity_type m_type;
    };

    inline constexpr entity::entity()
    : m_registry(nullptr), m_index(0), m_type(entity_type::Invalid)
    {
    }

    inline constexpr entity::operator size_t() const noexcept
    {
        return index();
    }

    inline constexpr entity::operator entity_type() const noexcept
    {
        return type();
    }

    inline entity::operator bool() const noexcept
    {
        return valid();
    }

    inline constexpr uint64_t entity::index() const noexcept
    {
        return m_index;
    }

    constexpr entity_type entity::type() const noexcept
    {
        return m_type;
    }

    inline bool entity::valid() const noexcept
    {
        return (m_registry != nullptr) && (m_type != entity_type::Invalid) && (m_index < m_registry->size());
    }

    constexpr entity::entity(entity_registry* reg, entity_index index, entity_type type)
    : m_registry(reg), m_index(index), m_type(type)
    {
        
    }

    template <typename T>
    inline entity::operator T&()
    {
        assert(valid());
        return m_registry->attribute_get<T>(m_index);
    }

    template <typename T, typename... Args>
    inline void entity::emplace(Args&&... args)
    {
        if (!valid())
        { return; }

        m_registry->attribute_emplace<T>(m_index, std::forward<Args>(args)...);
    }

    template <typename T>
    inline T& entity::get()
    {
        assert(valid());
        return m_registry->attribute_get<T>(m_index);
    }

    template <typename T>
    inline const T& entity::get() const
    {
        assert(valid());
        return std::as_const(get<T>());
    }

    template <typename T>
    inline int64_t entity::index() const noexcept
    {
        assert(valid());
        result<attribute_index> idx = m_registry->index_for_attribute<T>(m_index);
        if (!idx)
        { return -1; }
        return static_cast<int64_t>(*idx);
    }

} // namespace sz
