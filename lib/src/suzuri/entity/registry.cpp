// SPDX-FileCopyrightText: (c) rin contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "entity.hpp"
#include "registry.hpp"

namespace sz
{
    entity_registry::entity_registry()
    : m_attribute_map(), m_attributes(), m_count(0)
    {
    }

    entity entity_registry::entity_create(entity_type type)
    {
        return entity(this, m_count++, type);
    }

} // namespace sz
