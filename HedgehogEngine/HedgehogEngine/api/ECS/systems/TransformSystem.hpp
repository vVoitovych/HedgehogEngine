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
        /// Subscribe to TransformChangedEvent. Call once after system registration.
        HEDGEHOG_ENGINE_API void Init(EventBus& bus);

        /// Process pending entities and publish LocalMatrixUpdatedEvent for each.
        HEDGEHOG_ENGINE_API void Update(ECS::ECS& ecs, EventBus& bus);

    private:
        void OnTransformChanged(const TransformChangedEvent& event);

        std::vector<ECS::Entity> m_PendingEntities;
    };
}
