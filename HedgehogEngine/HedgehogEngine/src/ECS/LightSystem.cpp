#include "HedgehogEngine/api/ECS/systems/LightSystem.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"

namespace HedgehogEngine
{
    const std::vector<LightComponent>& LightSystem::GetLightComponents(ECS::ECS& ecs)
    {
        m_LightComponents.resize(m_Entities.size());
        for (size_t i = 0; i < m_Entities.size(); ++i)
            m_LightComponents[i] = ecs.GetComponent<LightComponent>(m_Entities[i]);
        return m_LightComponents;
    }

    void LightSystem::Update(ECS::ECS& ecs)
    {
        for (auto const& entity : m_Entities)
        {
            auto& light           = ecs.GetComponent<LightComponent>(entity);
            auto& transform       = ecs.GetComponent<TransformComponent>(entity);
            auto& transformMatrix = transform.m_ObjMatrix;

            light.m_Position  = transform.m_Position;
            HM::Vector3 dir   = { transformMatrix[0][0], transformMatrix[0][1], transformMatrix[0][2] };
            light.m_Direction = dir;

            if (light.m_CastShadows)
                m_ShadowDirection = dir;
        }
    }

    size_t LightSystem::GetLightComponentsCount() const
    {
        return m_Entities.size();
    }

    const LightComponent& LightSystem::GetLightComponentByIndex(const ECS::ECS& ecs, size_t index) const
    {
        return ecs.GetComponent<LightComponent>(m_Entities[index]);
    }

    void LightSystem::SetShadowCasting(const ECS::ECS& ecs, ECS::Entity inEntity, bool isCast)
    {
        m_ShadowDirection.reset();
        for (auto const& entity : m_Entities)
        {
            auto& light         = ecs.GetComponent<LightComponent>(entity);
            light.m_CastShadows = false;
            if (entity == inEntity && isCast)
            {
                light.m_CastShadows = true;
                m_ShadowDirection   = light.m_Direction;
            }
        }
    }

    const std::optional<HM::Vector3>& LightSystem::GetShadowDir() const
    {
        return m_ShadowDirection;
    }
}
