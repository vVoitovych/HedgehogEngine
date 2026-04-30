#include "HedgehogEngine/api/ECS/systems/HierarchySystem.hpp"
#include "ECS/api/components/Hierarchy.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"

namespace HedgehogEngine
{
    void HierarchySystem::Update(ECS::ECS& ecs)
    {
        UpdateChildrenMatrices(ecs, m_Root);
    }

    void HierarchySystem::SetRoot(ECS::Entity entity)
    {
        m_Root = entity;
    }

    void HierarchySystem::UpdateChildrenMatrices(ECS::ECS& ecs, ECS::Entity parent)
    {
        auto& transform = ecs.GetComponent<TransformComponent>(parent);
        auto& hierarchy = ecs.GetComponent<ECS::HierarchyComponent>(parent);

        for (auto const& entity : hierarchy.m_Children)
        {
            auto& childTransform       = ecs.GetComponent<TransformComponent>(entity);
            childTransform.m_ObjMatrix = transform.m_ObjMatrix * childTransform.m_ObjMatrix;
            UpdateChildrenMatrices(ecs, entity);
        }
    }
}
