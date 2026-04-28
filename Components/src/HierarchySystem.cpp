#include "Components/api/HierarchySystem.hpp"
#include "Components/api/HierarchyComponent.hpp"
#include "Components/api/TransformComponent.hpp"

namespace Scene
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
        auto& hierarchy = ecs.GetComponent<HierarchyComponent>(parent);

        for (auto const& entity : hierarchy.m_Children)
        {
            auto& childTransform       = ecs.GetComponent<TransformComponent>(entity);
            childTransform.m_ObjMatrix = transform.m_ObjMatrix * childTransform.m_ObjMatrix;
            UpdateChildrenMatrices(ecs, entity);
        }
    }
}
