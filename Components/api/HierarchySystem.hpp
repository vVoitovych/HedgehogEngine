#pragma once

#include "ECS/api/System.hpp"
#include "ECS/api/ECS.hpp"

namespace Scene
{
    class HierarchySystem : public ECS::System
    {
    public:
        void Update(ECS::ECS& ecs);
        void SetRoot(ECS::Entity entity);

    private:
        void UpdateChildrenMatrices(ECS::ECS& ecs, ECS::Entity parent);

    private:
        ECS::Entity m_Root = 0;
    };
}
