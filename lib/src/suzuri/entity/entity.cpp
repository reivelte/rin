// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "database.hpp"
#include "entity.hpp"

namespace sz
{
    static std::unordered_map<entity_attribute_type, u64> s_default_attr_to_non_native_ret;

    bool entity::has_default_type(entity_attribute_type attr)
    {
        if (attr != Null_Attribute)
        {
            for (const auto& opt : detail::s_default_attrs)
            {
                if (opt)
                {
                    const auto& p = *opt;
                    if (p.first == attr) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    entity::entity() :
        m(new entity::data{
            .attributes{},
            .type = sz::entity_type::Invalid,
            .indexed = false,
            .has_children = false
        })
    {
        set_attribute(Name, "");
    }

    entity::entity(const std::string& name, sz::entity_type type) :
        m(new entity::data{
            .attributes{},
            .type = type,
            .indexed = false,
            .has_children = false
        })
    {
        set_attribute(Name, name);
    }

    entity::~entity()
    {
    }

    bool entity::erase_attribute(entity_attribute_type attr)
    {
        return m->attributes.erase(attr) == 1;
    }

    const entity_attribute& entity::attribute(entity_attribute_type attr) const
    {
        return m->attributes.at(attr);
    }

    const entity_attribute_map& entity::attribute_map() const
    {
        return m->attributes;
    }

    entity_type entity::type() const
    {
        return m->type;
    }

    bool entity::valid() const
    {
        if (has_attribute(Name))
        {
            if (attribute(Name).is_native_type()) {
                return attribute<string>(Name).size() && type() != entity_type::Invalid;
            }
            return type() != entity_type::Invalid;
        }
        return false;
    }

    bool entity::has_attribute(entity_attribute_type attr) const
    {
        return m->attributes.contains(attr);
    }

    bool entity::is_indexed() const
    {
        return m->indexed;
    }

    bool entity::has_children() const
    {
        return m->has_children;
    }

    void entity::set_type(entity_type type)
    {
        m->type = type;
    }

    void entity::set_indexed(bool indexed)
    {
        m->indexed = indexed;
    }

    void entity::set_has_children(bool has_children)
    {
        m->has_children = has_children;
    }

    sz::result<void> entity::commit_to(entity_database &db)
    {
        using enum entity_type;

        const auto& id = attribute<string>(Id);
        const auto& desc = attribute<string>(Description);
        const int color = attribute<int>(Color);
        if (db.entity_exists(id))
        {
            auto fmt = std::format("rgba={},description={}", color, desc);
            auto r = db.update_entity(id, utility::split(",", fmt));
            if (r.code != result_code::Success) {
                return common_error(r.code);
            }
        }
        else
        {
            auto t = type();
            usize ctime = has_attribute(Created_Int) ? attribute<usize>(Created_Int) : 0;
            usize mtime = has_attribute(Modified_Int) ? attribute<usize>(Modified_Int) : 0;
            entity_database_info info{
                .extra_info{},
                .id = id,
                .description = desc,
                .createtime = ctime,
                .modtime = mtime,
                .rgba = color,
                .type = t
            };
            if (t == File)
            {
                info.extra_info.emplace_back(ctime);
                info.extra_info.emplace_back(mtime);
            }
            // FIXME (entity_database): old code that expects info.id to reference a tag entity and extra_info[0] to be new alias id
            // else if (t == Alias)
            // {
            // }
            if (auto rc = db.register_entity(info); rc != result_code::Success) {
                return common_error(rc);
            }
        }
        return {};
    }

    bool entity::contains_non_native_default(entity_attribute_type attr)
    {
        return s_default_attr_to_non_native_ret.contains(attr);
    }

    void entity::emplace_non_native_default(entity_attribute_type attr, u64 h)
    {
        s_default_attr_to_non_native_ret.emplace(attr, h);
    }
    
    const std::unordered_map<entity_attribute_type, u64>& entity::non_native_defaults()
    {
        return s_default_attr_to_non_native_ret;
    }
} // namespace sz
