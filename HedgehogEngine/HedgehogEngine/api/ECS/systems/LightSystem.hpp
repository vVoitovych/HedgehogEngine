#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"

#include "ECS/api/System.hpp"
#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"
#include "HedgehogEngine/api/ECS/components/LightComponent.hpp"

#include <vector>
#include <optional>

namespace HedgehogEngine
{
    class LightSystem : public ECS::System
    {
    public:
        // Subscribes to WorldMatrixUpdatedEvent on the ECS's EventBus service, when one is registered.
        HEDGEHOG_ENGINE_API void OnRegister(ECS::ECS& ecs) override;
        HEDGEHOG_ENGINE_API void OnUnregister(ECS::ECS& ecs) override;

        HEDGEHOG_ENGINE_API const std::vector<LightComponent>& GetLightComponents(ECS::ECS& ecs);
        HEDGEHOG_ENGINE_API void   Update(ECS::ECS& ecs);

        HEDGEHOG_ENGINE_API void SetShadowCasting(const ECS::ECS& ecs, ECS::Entity entity, bool isCast);
        HEDGEHOG_ENGINE_API const std::optional<HM::Vector3>& GetShadowDir() const;

    private:
        void OnWorldMatrixUpdated(const WorldMatrixUpdatedEvent& event);

        EventBus*      m_Bus          = nullptr;
        SubscriptionId m_Subscription = SubscriptionId::Invalid;

    private:
        std::vector<LightComponent> m_LightComponents;
        std::optional<HM::Vector3>  m_ShadowDirection;
        std::vector<ECS::Entity>    m_PendingEntities;
    };
}
