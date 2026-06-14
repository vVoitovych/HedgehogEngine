#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"

#include "ECS/api/System.hpp"
#include "ECS/api/ECS.hpp"

#include <unordered_set>
#include <vector>

namespace HedgehogEngine
{
    class HierarchySystem : public ECS::System
    {
    public:
        /// Subscribe to LocalMatrixUpdatedEvent. Call once after system registration.
        HEDGEHOG_ENGINE_API void Init(EventBus& bus);

        /// Cascade world matrices for all queued subtrees; no-op when queue is empty.
        HEDGEHOG_ENGINE_API void Update(ECS::ECS& ecs);

    private:
        void OnLocalMatrixUpdated(const LocalMatrixUpdatedEvent& event);

        void CascadeSubtree(ECS::ECS& ecs, ECS::Entity parent, bool parentWorldUpdated,
                            const std::unordered_set<ECS::Entity>& pending);

        std::vector<ECS::Entity> m_PendingUpdates;
    };
}
