#pragma once

#include "ECS/api/Entity.hpp"
#include <vector>
#include <string>

namespace Scene
{
    class HierarchyComponent
    {
    public:
        std::string              m_Name;
        ECS::Entity              m_Parent = 0;
        std::vector<ECS::Entity> m_Children;
    };
}
