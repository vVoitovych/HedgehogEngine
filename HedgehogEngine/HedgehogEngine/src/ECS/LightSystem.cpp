#include "HedgehogEngine/api/ECS/systems/LightSystem.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"

#include <algorithm>

namespace HedgehogEngine
{
    void LightSystem::Init(EventBus& bus)
    {
        bus.Subscribe<WorldMatrixUpdatedEvent>([this](const WorldMatrixUpdatedEvent& e)
        {
            OnWorldMatrixUpdated(e);
        });
    }

    void LightSystem::OnWorldMatrixUpdated(const WorldMatrixUpdatedEvent& event)
    {
        const auto& entities = GetEntities();
        if (std::find(entities.begin(), entities.end(), event.entity) != entities.end())
            m_PendingEntities.push_back(event.entity);
    }

    const std::vector<LightComponent>& LightSystem::GetLightComponents(ECS::ECS& ecs)
    {
        m_LightComponents.resize(m_Entities.size());
        for (size_t i = 0; i < m_Entities.size(); ++i)
            m_LightComponents[i] = ecs.GetComponent<LightComponent>(m_Entities[i]);
        return m_LightComponents;
    }

    void LightSystem::Update(ECS::ECS& ecs)
    {
        if (m_PendingEntities.empty())
            return;

        for (auto const& entity : m_PendingEntities)
        {
            auto& light           = ecs.GetComponent<LightComponent>(entity);
            auto& transform       = ecs.GetComponent<TransformComponent>(entity);
            auto& transformMatrix = transform.ObjMatrix;

            light.Position  = transform.Position;
            HM::Vector3 dir   = { transformMatrix[0][0], transformMatrix[0][1], transformMatrix[0][2] };
            light.Direction = dir;

            if (light.CastShadows)
                m_ShadowDirection = dir;
        }
        m_PendingEntities.clear();
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
            light.CastShadows = false;
            if (entity == inEntity && isCast)
            {
                light.CastShadows = true;
                m_ShadowDirection   = light.Direction;
            }
        }
    }

    const std::optional<HM::Vector3>& LightSystem::GetShadowDir() const
    {
        return m_ShadowDirection;
    }
}
