// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <any>
#include <array>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <iostream>
#include <sz_export.hpp>
#include "../utility/string.hpp"
#include "../utility/shared_data.hpp"
#include "../exception.hpp"
#include "../types.hpp"
#include "../vector.hpp"
#include "../file.hpp"
#include "../string.hpp"
#include "common.hpp"

namespace sz
{
    class entity;
    class entity_database;

    enum class entity_attribute_native_type : int
    {
        None, String, Vector2, File, UInt, Int
    };

    template <typename T>
    consteval u64 hash_for()
    {
        return utility::hash_string(type_name<T>());
    }

#define SZ_DEFINE_ENTITY_ATTR_DEFAULTS(...) \
    namespace detail \
    { \
        using enum entity_attribute_type; \
        typedef std::optional<std::pair<entity_attribute_type, u64>> default_entity_attribute; \
        template <typename... Types, std::size_t N = sizeof...(Types)> \
        consteval std::array<default_entity_attribute, N> init_default_attrs() \
        { \
            std::array<default_entity_attribute, N> ret{ \
            __VA_ARGS__ \
            }; \
            return ret; \
        } \
    }

#define SZ_DEFINE_ENTITY_ATTR_TYPESTRINGS(...) \
    namespace detail \
    { \
        template <typename... Types, std::size_t N = sizeof...(Types)> \
        consteval std::array<std::pair<u64, std::string_view>, N> init_attr_typestrings() \
        { \
            std::array<std::pair<u64, std::string_view>, N> ret{ \
                __VA_ARGS__ \
            }; \
            return ret; \
        }; \
    }

    #define SZ_TYPESTRING(T, X) \
    []() -> auto \
    { \
        return std::make_pair(hash_for<T>(), X); \
    }()

#define SZ_ATTR(T, X) \
    []() -> auto \
    { \
        return std::make_pair(X, hash_for<T>()); \
    }()

#define SZ_DEFINE_ENTITY_ATTRIBUTES(...) \
    namespace detail \
    { \
        typedef std::variant< \
            __VA_ARGS__, \
            std::any \
        > \
        entity_attribute_base; \
        \
        template <typename T> \
        concept is_entity_attribute = requires \
        { \
            requires \
            (is_any_of<T, __VA_ARGS__>) || \
            is_int_compatible<T> || \
            std::convertible_to<T, sz::string>; \
        }; \
        static constexpr auto s_default_attrs = init_default_attrs<__VA_ARGS__>(); \
        static constexpr auto s_typestrings = init_attr_typestrings<__VA_ARGS__>(); \
        template <typename T> \
        constexpr std::string_view typestring_for() \
        { \
            const u64 h = hash_for<T>(); \
            for (const auto& p : s_typestrings) \
            { \
                if (p.first == h) { \
                    return p.second; \
                } \
            } \
            return std::string_view(); \
        } \
    } \
    using detail::is_entity_attribute

    static constexpr std::string_view TYPE_STR = "string";
    static constexpr std::string_view TYPE_VEC2 = "vector2";
    static constexpr std::string_view TYPE_FILE = "file";
    static constexpr std::string_view TYPE_USIZE = "usize";
    static constexpr std::string_view TYPE_INT = "int";

#define SZ_ENTITY_ATTR_NATIVE_TYPE_LIST \
    string, \
    vector2, \
    file, \
    usize, \
    i64

    SZ_DEFINE_ENTITY_ATTR_TYPESTRINGS(
        SZ_TYPESTRING(string, TYPE_STR),
        SZ_TYPESTRING(vector2, TYPE_VEC2),
        SZ_TYPESTRING(file, TYPE_FILE),
        SZ_TYPESTRING(usize, TYPE_USIZE),
        SZ_TYPESTRING(i64, TYPE_INT)
    );

    SZ_DEFINE_ENTITY_ATTR_DEFAULTS(
        SZ_ATTR(string, Name),
        SZ_ATTR(vector2, Sizehint),
        SZ_ATTR(file, File_Info)
    );

    SZ_DEFINE_ENTITY_ATTRIBUTES(
        SZ_ENTITY_ATTR_NATIVE_TYPE_LIST
    );

    class entity_attribute : public detail::entity_attribute_base
    {
        using base = detail::entity_attribute_base;
        using base::base;

        public:
        template <is_entity_attribute T>
        entity_attribute(const T& val) :
            base([&]() -> auto // FIXME: two copies end up being made (once in return from lambda and again inside variant's constructor)
        {
            set_hash<T>();
            if constexpr (std::convertible_to<T, string> && !is_any_of<T, SZ_ENTITY_ATTR_NATIVE_TYPE_LIST>)
            {
                m_native_type = String;
                return string(val);
            }
            else if constexpr (is_int_compatible<T> && !is_any_of<T, SZ_ENTITY_ATTR_NATIVE_TYPE_LIST>)
            {
                if constexpr (std::unsigned_integral<T>) {
                    m_native_type = UInt;
                    return static_cast<usize>(val);
                }
                else {
                    m_native_type = Int;
                    return static_cast<i64>(val);
                }
            }
            else {
                auto s = detail::typestring_for<T>();
                if (s == TYPE_STR) { m_native_type = String; }
                else if (s == TYPE_VEC2) { m_native_type = Vector2; }
                else if (s == TYPE_FILE) { m_native_type = File; }
                else if (s == TYPE_USIZE) { m_native_type = UInt; }
                else if (s == TYPE_INT) { m_native_type = Int; }
                else { m_native_type = None; } // this shouldn't happen (in theory)
                return val;
            }
        }())
        {
        }

        template <typename T> requires (!std::same_as<T, std::any> && !is_entity_attribute<T>)
        entity_attribute(const T& val) :
            base(std::any(val)), m_native_type(None)
        {
            set_hash<T>();
        }

        explicit entity_attribute(const std::any&) = delete;

        entity_attribute(const std::any& val, u64 hash) :
            base(val), m_hash(hash), m_native_type(None)
        {
        }

        template <typename T>
        inline T get() const
        {
            bool match = hash() == hash_for<T>();
            if constexpr (!is_entity_attribute<T>)
            {
                if (match) {
                    return std::any_cast<T>(std::get<std::any>(*this));
                }
            }
            else if (match) {
                return std::get<T>(*this);
            }
            throw exception(result_code::Interface_Misuse, "type mismatch");
        }

        inline bool is_native_type() const noexcept { return m_native_type != None; }
        inline entity_attribute_native_type native_type() const noexcept { return m_native_type; }
        inline u64 hash() const noexcept { return m_hash; }

        protected:
        friend class entity;

        template<typename T>
        inline void set_hash() { m_hash = hash_for<T>(); }

        private:
        using enum entity_attribute_native_type;

        u64 m_hash;
        entity_attribute_native_type m_native_type;
    };
    typedef std::unordered_map<entity_attribute_type, entity_attribute> entity_attribute_map;

    namespace detail
    {
        // TODO: good way to be able to change the types here using the definition macros above
        template <is_int_compatible T>
        T integral_cast(const entity_attribute& v)
        {
            using enum sz::entity_attribute_native_type;

            if (std::holds_alternative<usize>(v)) {
                return static_cast<T>(std::get<usize>(v));
            }
            else if (std::holds_alternative<i64>(v)) {
                return static_cast<T>(std::get<i64>(v));
            }
            else if (std::holds_alternative<std::any>(v))
            {
                // we have a problem if T does not match what is stored inside the any
                return std::any_cast<T>(std::get<std::any>(v));
            }
            return T{};
        }

    } // namespace detail

    class SZ_API entity
    {
        public:
        template <typename Ret, is_entity_attribute Stored, typename Func> requires (is_function_of<Func, Ret, const Stored&>)
        static void set_adaptor(Func adaptor);

        template <is_entity_attribute Ret, typename Func> requires (is_function_of<Func, Ret, const entity_attribute&>)
        static void set_adaptor(Func adaptor);

        template <typename Ret>
        static void remove_adaptor();

        template <typename Ret>
        static bool has_adaptor();

        template <typename Ret>
        static void set_non_native_default(entity_attribute_type attr);

        static bool has_default_type(entity_attribute_type attr);

        entity();
        entity(const string& name, entity_type type);
        ~entity();

        template <is_entity_attribute T>
        void set_attribute(entity_attribute_type attr, const T& value);

        template <typename T> requires (!is_entity_attribute<T>)
        void set_attribute(entity_attribute_type attr, const T& value);

        template <typename T> requires (!is_any_of<T, SZ_ENTITY_ATTR_NATIVE_TYPE_LIST>)
        bool assign_attribute(entity_attribute_type attr, const T& value);

        bool erase_attribute(entity_attribute_type attr);

        template <is_entity_attribute T> requires (is_any_of<T, SZ_ENTITY_ATTR_NATIVE_TYPE_LIST>)
        T& attribute(entity_attribute_type attr = Null_Attribute);
        
        template <is_entity_attribute T> requires (is_any_of<T, SZ_ENTITY_ATTR_NATIVE_TYPE_LIST>)
        const T& attribute(entity_attribute_type attr = Null_Attribute) const;

        template <typename T> requires (!is_entity_attribute<T> || (!is_any_of<T, SZ_ENTITY_ATTR_NATIVE_TYPE_LIST> && is_int_compatible<T>))
        T attribute(entity_attribute_type attr = Null_Attribute) const;

        const entity_attribute& attribute(entity_attribute_type attr) const;

        const entity_attribute_map& attribute_map() const;
        entity_type type() const;
        bool valid() const;
        bool has_attribute(entity_attribute_type attr) const;
        bool is_indexed() const;
        bool has_children() const;

        protected:
        void set_type(entity_type type);
        void set_indexed(bool indexed);
        void set_has_children(bool has_children);
        sz::result<void> commit_to(entity_database& db);

        private:
        using enum entity_attribute_type;
        friend class entity_database;

        static bool contains_non_native_default(entity_attribute_type attr);
        static void emplace_non_native_default(entity_attribute_type attr, u64 h);
        static const std::unordered_map<entity_attribute_type, u64>& non_native_defaults();

        template <typename T>
        entity_attribute_type m_default_attr_for_type() const noexcept;

        template <typename T>
        entity_attribute_type m_ensure_attr(entity_attribute_type attr) const;

        private:
        // TODO: see if we can stop relying on statics here
        static inline std::unordered_map<u64, std::any> s_adaptors;
        static inline std::unordered_map<u64, std::string_view> s_ret_to_stored_typestring;

        struct data
        {
            entity_attribute_map attributes;
            entity_type type;
            bool indexed;
            bool has_children;
        };
        utility::shared_implicit<data> m;
    };

    template <typename Ret, is_entity_attribute Stored, typename Func> requires (is_function_of<Func, Ret, const Stored&>)
    inline void entity::set_adaptor(Func adaptor)
    {
        constexpr u64 h = hash_for<Ret>();
        const auto str = detail::typestring_for<Stored>();
        s_ret_to_stored_typestring.emplace(h, str);
        s_adaptors.emplace(h, std::function<Ret(Stored)>(adaptor));
    }

    template <is_entity_attribute Ret, typename Func> requires (is_function_of<Func, Ret, const entity_attribute&>)
    inline void entity::set_adaptor(Func adaptor)
    {
        s_adaptors.emplace(hash_for<Ret>(), std::function<Ret(entity_attribute)>(adaptor));
    }
    
    template <typename Ret>
    inline void entity::remove_adaptor()
    {
        const u64 h = hash_for<Ret>();
        s_ret_to_stored_typestring.erase(h);
        s_adaptors.erase(h);
    }

    template <typename Ret>
    inline bool entity::has_adaptor()
    {
        return s_adaptors.contains(hash_for<Ret>());
    }

    template <typename Ret>
    inline void entity::set_non_native_default(entity_attribute_type attr)
    {
        emplace_non_native_default(attr, hash_for<Ret>());
    }

    template <is_entity_attribute T>
    inline void entity::set_attribute(entity_attribute_type attr, const T& value)
    {
        entity_attribute v(value);
        v.set_hash<T>();
        m->attributes.emplace(m_ensure_attr<T>(attr), std::move(v));
    }

    // returns true if attr exists in the attribute map
    // returns false if the attribute was not able to be assigned
    template <typename T> requires (!is_any_of<T, SZ_ENTITY_ATTR_NATIVE_TYPE_LIST>)
    inline bool entity::assign_attribute(entity_attribute_type attr, const T& value)
    {
        if (m->attributes.contains(attr))
        {
            m->attributes.insert_or_assign(attr, value);
            return true;
        }
        return false;
    }

    template <typename T> requires (!is_entity_attribute<T>)
    inline void entity::set_attribute(entity_attribute_type attr, const T& value)
    {
        if (attr == Null_Attribute) {
            return;
        }
        const u64 h = hash_for<T>(); 

        if (has_default_type(attr) && !contains_non_native_default(attr))
        {
            // trying to set a non-native type for an attribute that already has a default native one
            emplace_non_native_default(attr, h);
        }
        m->attributes.emplace(attr, entity_attribute(std::any(value), h));
    }

    template <is_entity_attribute T> requires (is_any_of<T, SZ_ENTITY_ATTR_NATIVE_TYPE_LIST>)
    inline T& entity::attribute(entity_attribute_type attr)
    {
        auto& v = m->attributes.at(m_ensure_attr<T>(attr));
        const u64 h = hash_for<T>();
        if (v.hash() == h) {
            return std::get<T>(v);
        }
        else if (s_adaptors.contains(h))
        {
            T nv = std::any_cast<std::function<T(entity_attribute)>>(s_adaptors.at(h))(v);
            m->attributes.erase(attr);
            set_attribute<T>(attr, nv);
            return std::get<T>(m->attributes.at(attr));
        }
        throw exception(result_code::Interface_Misuse, "type mismatch");
    }

    template <is_entity_attribute T> requires (is_any_of<T, SZ_ENTITY_ATTR_NATIVE_TYPE_LIST>)
    inline const T& entity::attribute(entity_attribute_type attr) const
    {
        return std::get<T>(m->attributes.at(m_ensure_attr<T>(attr)));
    }

    // this function is only ideal for use with implicitly shared types or primitive types
    template <typename T> requires (!is_entity_attribute<T> || (!is_any_of<T, SZ_ENTITY_ATTR_NATIVE_TYPE_LIST> && is_int_compatible<T>))
    inline T entity::attribute(entity_attribute_type attr) const
    {
        attr = m_ensure_attr<T>(attr);
        if (m->attributes.contains(attr))
        {
            if constexpr (is_int_compatible<T>) {
                return detail::integral_cast<T>(m->attributes.at(attr));
            }
            if (const auto& x = m->attributes.at(attr); std::holds_alternative<std::any>(x))
            {
                if (const std::any& a = std::get<std::any>(x); a.type() == typeid(T)) {
                    return std::any_cast<T>(a);
                }
            }
            else if (const u64 h = hash_for<T>(); s_adaptors.contains(h))
            {
                auto adapt_from_impl = [&] <typename Stored> (const entity_attribute& v) -> T
                {
                    return std::any_cast<std::function<T(Stored)>>(s_adaptors.at(h))(std::get<Stored>(v));
                };

                // TODO: fit this into the class construction macros above
    #define adapt_from(S, X) adapt_from_impl.template operator()<S>(X)
                const auto typestring = s_ret_to_stored_typestring.at(h);

                if (typestring == TYPE_STR)         { return adapt_from(string, x); }
                else if (typestring == TYPE_VEC2)   { return adapt_from(vector2, x); }
                else if (typestring == TYPE_FILE)   { return adapt_from(file, x); }
                else if (typestring == TYPE_USIZE)  { return adapt_from(usize, x); }
                else if (typestring == TYPE_INT)    { return adapt_from(i64, x); }
    #undef adapt_from
            }
        }
        return T();
    }

    template <typename T>
    inline entity_attribute_type entity::m_default_attr_for_type() const noexcept
    {
        const u64 h = hash_for<T>();

        for (const auto& opt : detail::s_default_attrs)
        {
            if (opt)
            {
                if (const auto& p = *opt; p.second == h) {
                    return p.first;
                }
            }
        }

        // T might be a non-native type with an assigned default attribute
        for (const auto& [attr, non_native_ret] : non_native_defaults())
        {
            if (non_native_ret == h) {
                return attr;
            }
        }
        return Null_Attribute;
    }

    template <typename T>
    inline entity_attribute_type entity::m_ensure_attr(entity_attribute_type attr) const
    {
        attr = attr != Null_Attribute ? attr : m_default_attr_for_type<T>();
        assert(attr != Null_Attribute);
        return attr;
    }

} // namespace sz
