#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"

#include "ECS/api/System.hpp"
#include "ECS/api/ECS.hpp"

#include <vector>

namespace HedgehogEngine
{
    class TransformSystem : public ECS::System
    {
    public:
        // Subscribes to TransformChangedEvent on the ECS's EventBus service, when one is registered.
        HEDGEHOG_ENGINE_API void OnRegister(ECS::ECS& ecs) override;
        HEDGEHOG_ENGINE_API void OnUnregister(ECS::ECS& ecs) override;

        // Runs Update in the Transform phase, in every play mode, with the EventBus service.
        ECS::SystemPhase         GetPhase() const override { return ECS::SystemPhase::Transform; }
        HEDGEHOG_ENGINE_API void OnFrame(ECS::ECS& ecs, const ECS::FrameContext& ctx) override;

        /// Process pending entities and publish LocalMatrixUpdatedEvent for each.
        HEDGEHOG_ENGINE_API void Update(ECS::ECS& ecs, EventBus& bus);

    private:
        void OnTransformChanged(const TransformChangedEvent& event);

        EventBus*      m_Bus          = nullptr;
        SubscriptionId m_Subscription = SubscriptionId::Invalid;

        std::vector<ECS::Entity> m_PendingEntities;
    };
}
