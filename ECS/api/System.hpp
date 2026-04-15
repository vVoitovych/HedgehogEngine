#pragma once

#include "Entity.hpp"

#include <vector>

namespace ECS
{
    class SystemManager;

    class System
    {
        friend class SystemManager;
    public:
        const std::vector<Entity>& GetEntities() const { return m_Entities; }
    protected:
        std::vector<Entity> m_Entities;
    };
}
