#pragma once

#include "ECS/api/Entity.hpp"

#include <string>
#include <vector>

namespace ECS
{
    struct HierarchyComponent
    {
        std::string         Name;
        Entity              Parent{0};
        std::vector<Entity> Children;
    };
}
