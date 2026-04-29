#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "ECS/api/System.hpp"
#include "ECS/api/ECS.hpp"

namespace Scene
{
    class HierarchySystem : public ECS::System
    {
    public:
        HEDGEHOG_ENGINE_API void Update(ECS::ECS& ecs);
        HEDGEHOG_ENGINE_API void SetRoot(ECS::Entity entity);

    private:
        void UpdateChildrenMatrices(ECS::ECS& ecs, ECS::Entity parent);

    private:
        ECS::Entity m_Root = 0;
    };
}
