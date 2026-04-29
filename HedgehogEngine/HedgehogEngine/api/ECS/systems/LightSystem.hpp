#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "ECS/api/System.hpp"
#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"
#include "HedgehogEngine/api/ECS/components/LightComponent.hpp"

#include <vector>
#include <optional>

namespace Scene
{
    class LightSystem : public ECS::System
    {
    public:
        HEDGEHOG_ENGINE_API const std::vector<LightComponent>& GetLightComponents(ECS::ECS& ecs);
        HEDGEHOG_ENGINE_API void   Update(ECS::ECS& ecs);
        HEDGEHOG_ENGINE_API size_t GetLightComponentsCount() const;
        HEDGEHOG_ENGINE_API const LightComponent& GetLightComponentByIndex(const ECS::ECS& ecs, size_t index) const;

        HEDGEHOG_ENGINE_API void SetShadowCasting(const ECS::ECS& ecs, ECS::Entity entity, bool isCast);
        HEDGEHOG_ENGINE_API const std::optional<HM::Vector3>& GetShadowDir() const;

    private:
        std::vector<LightComponent> m_LightComponents;
        std::optional<HM::Vector3>  m_ShadowDirection;
    };
}
