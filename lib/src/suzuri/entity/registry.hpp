// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <unordered_map>
#include <any>
#include <vector>
#include <span>
#include <typeinfo>
#include <typeindex>
#include <tuple>
#include <print>
#include <sz_export.hpp>
#include "../result.hpp"
#include "common.hpp"

namespace sz
{
    class entity;
    class SZ_API entity_registry
    {
        public:
        entity_registry();
        ~entity_registry() = default;

        entity entity_create(entity_type type);
        inline void entity_reserve(size_t size);
        inline void clear() noexcept;
        inline size_t size() const noexcept;

        template <typename T> void attribute_reserve(size_t size);

        template <typename T, typename... Args>
        std::tuple<T&, attribute_index> attribute_emplace(entity_index index, Args&&... args);
        template <typename T> T& attribute_get(entity_index index);
        
        template <typename T> result<attribute_index> index_for_attribute(entity_index index) const;

        template <typename T> std::span<T> view();
        template <typename T> std::span<const T> view() const;

        private:
        template <typename T> void m_map_attribute(entity_index entity_idx, attribute_index attr_idx);
        template <typename T> std::vector<T>& m_vector_for_type();

        private:
        std::vector<std::unordered_map<std::type_index, attribute_index>> m_attribute_map; // index is entity_index
        std::unordered_map<std::type_index, std::any> m_attributes;
        size_t m_count;
    };

    inline void entity_registry::entity_reserve(size_t size)
    {
        m_attribute_map.reserve(size);
    }

    inline void entity_registry::clear() noexcept
    {
        m_attributes.clear();
        m_count = 0;
    }

    inline size_t entity_registry::size() const noexcept
    {
        return m_count;
    }

    template <typename T>
    inline void entity_registry::attribute_reserve(size_t size)
    {
        auto& array = m_vector_for_type<T>();
        array.reserve(size);
    }

    template <typename T, typename... Args>
    inline std::tuple<T &, attribute_index> entity_registry::attribute_emplace(entity_index index, Args &&...args)
    {
        auto& array = m_vector_for_type<T>();
        array.emplace_back(std::forward<Args>(args)...);

        const attribute_index attr_idx = array.size() - 1;
        m_map_attribute<T>(index, attr_idx);

        return { array[attr_idx], attr_idx };
    }

    template <typename T>
    inline T& entity_registry::attribute_get(entity_index index)
    {
        const auto attr_idx = index_for_attribute<T>(index);
        
        if (!attr_idx)
        { throw std::invalid_argument("no such attribute"); }

        return m_vector_for_type<T>()[*attr_idx];
    }

    template <typename T>
    inline result<attribute_index> entity_registry::index_for_attribute(entity_index index) const
    {
        if (index < m_attribute_map.size())
        {
            const auto& map = m_attribute_map[index];
            const std::type_index typeindex(typeid(T));
            if (map.contains(typeindex))
            { return map.at(typeindex); }
            else
            { return common_error(result_code::Invalid_Argument); }
        }
        return common_error(result_code::Index_Out_Of_Bounds);
    }

    template <typename T>
    inline std::span<T> entity_registry::view()
    {
        auto& array = m_vector_for_type<T>();
        return std::span<T>(array.begin(), array.end());
    }

    template <typename T>
    inline std::span<const T> entity_registry::view() const
    {
        const auto& array = std::as_const(m_vector_for_type<T>());
        return std::span<const T>(array.cbegin(), array.cend());
    }

    template <typename T>
    inline void entity_registry::m_map_attribute(entity_index entity_idx, attribute_index attr_idx)
    {
        if (entity_idx >= m_attribute_map.size())
        { m_attribute_map.resize(entity_idx + 1); }

        m_attribute_map[entity_idx].emplace(std::type_index(typeid(T)), attr_idx);
    }

    // template <typename T>
    // inline attribute_index entity_registry::m_index_for_attribute(entity_index entity_idx) const
    // {
    //     if (entity_idx < m_attribute_map.size())
    //     {
    //         const auto& map = m_attribute_map[entity_idx];
    //         const std::type_index typeindex(typedid(T));
    //         if (map.contains(typeindex))
    //         { return map.at(typeindex); }
    //     }
    //     std::println("[entity_registry] warning: no index for the given attribute");
    //     return 0; // TODO
    // }

    template <typename T>
    inline std::vector<T> &entity_registry::m_vector_for_type()
    {
        const std::type_index typeindex(typeid(T));

        if (!m_attributes.contains(typeindex))
        { m_attributes.emplace(typeindex, std::vector<T>()); }

        return std::any_cast<std::vector<T>&>(m_attributes[typeindex]);
    }

} // namespace sz
