#pragma once

#include "ECS/api/Entity.hpp"

#include <string>
#include <vector>

namespace ECS
{
    struct HierarchyComponent
    {
        std::string         m_Name;
        Entity              m_Parent{0};
        std::vector<Entity> m_Children;
    };
}
