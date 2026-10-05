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
        // Subscribes to LocalMatrixUpdatedEvent on the ECS's EventBus service, when one is registered.
        HEDGEHOG_ENGINE_API void OnRegister(ECS::ECS& ecs) override;
        HEDGEHOG_ENGINE_API void OnUnregister(ECS::ECS& ecs) override;

        // Runs Update in the Transform phase, in every play mode, with the EventBus service.
        ECS::SystemPhase         GetPhase() const override { return ECS::SystemPhase::Transform; }
        HEDGEHOG_ENGINE_API void OnFrame(ECS::ECS& ecs, const ECS::FrameContext& ctx) override;

        /// Cascade world matrices for all queued subtrees; publishes WorldMatrixUpdatedEvent per entity.
        HEDGEHOG_ENGINE_API void Update(ECS::ECS& ecs, EventBus& bus);

    private:
        void OnLocalMatrixUpdated(const LocalMatrixUpdatedEvent& event);

        EventBus*      m_Bus          = nullptr;
        SubscriptionId m_Subscription = SubscriptionId::Invalid;

        void CascadeSubtree(ECS::ECS& ecs, ECS::Entity parent, bool parentWorldUpdated,
                            const std::unordered_set<ECS::Entity>& pending, EventBus& bus);

        std::vector<ECS::Entity> m_PendingUpdates;
    };
}
