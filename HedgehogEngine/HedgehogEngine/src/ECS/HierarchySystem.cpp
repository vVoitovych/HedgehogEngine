#include "HedgehogEngine/api/ECS/systems/HierarchySystem.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"

#include "ECS/api/components/Hierarchy.hpp"

namespace HedgehogEngine
{
    void HierarchySystem::Init(EventBus& bus)
    {
        bus.Subscribe<LocalMatrixUpdatedEvent>([this](const LocalMatrixUpdatedEvent& e)
        {
            OnLocalMatrixUpdated(e);
        });
    }

    void HierarchySystem::OnLocalMatrixUpdated(const LocalMatrixUpdatedEvent& event)
    {
        m_PendingUpdates.push_back(event.entity);
    }

    void HierarchySystem::Update(ECS::ECS& ecs, EventBus& bus)
    {
        if (m_PendingUpdates.empty())
            return;

        const std::unordered_set<ECS::Entity> pending(m_PendingUpdates.begin(), m_PendingUpdates.end());
        m_PendingUpdates.clear();

        ECS::Entity root    = ecs.GetRoot();
        auto& rootTransform = ecs.GetComponent<TransformComponent>(root);

        const bool rootUpdated = pending.count(root) > 0;
        if (rootUpdated)
        {
            rootTransform.ObjMatrix = rootTransform.LocalMatrix;
            bus.Publish(WorldMatrixUpdatedEvent{ root });
        }

        CascadeSubtree(ecs, root, rootUpdated, pending, bus);
    }

    void HierarchySystem::CascadeSubtree(ECS::ECS& ecs, ECS::Entity parent, bool parentWorldUpdated,
                                          const std::unordered_set<ECS::Entity>& pending, EventBus& bus)
    {
        auto& parentTransform = ecs.GetComponent<TransformComponent>(parent);
        auto& hierarchy       = ecs.GetComponent<ECS::HierarchyComponent>(parent);

        for (auto const& child : hierarchy.Children)
        {
            auto& childTransform        = ecs.GetComponent<TransformComponent>(child);
            const bool needsWorldUpdate = parentWorldUpdated || pending.count(child) > 0;

            if (needsWorldUpdate)
            {
                childTransform.ObjMatrix = parentTransform.ObjMatrix * childTransform.LocalMatrix;
                bus.Publish(WorldMatrixUpdatedEvent{ child });
            }

            CascadeSubtree(ecs, child, needsWorldUpdate, pending, bus);
        }
    }
}
