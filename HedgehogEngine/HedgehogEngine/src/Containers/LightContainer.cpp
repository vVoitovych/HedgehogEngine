#include "HedgehogEngine/api/Containers/LightContainer.hpp"

#include "HedgehogEngine/api/ECS/systems/LightSystem.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>

namespace HedgehogEngine
{
    LightContainer::LightContainer()
    {
        m_Lights.resize(MAX_LIGHTS_COUNT);
    }

    void LightContainer::UpdateLights(const ECS::ECS& ecs, const LightSystem& lightSystem)
    {
        auto lightComponentsCount = lightSystem.GetLightComponentsCount();
        if (lightComponentsCount > MAX_LIGHTS_COUNT)
        {
            LOGWARNING("Too many light components. Some of them will not be processed!");
        }
        lightComponentsCount = std::min(lightComponentsCount, static_cast<size_t>(MAX_LIGHTS_COUNT));

        size_t counter = 0;
        for (size_t i = 0; i < lightComponentsCount; ++i)
        {
            const auto& lc = lightSystem.GetLightComponentByIndex(ecs, i);
            if (!lc.m_Enable)
                continue;

            auto& light       = m_Lights[counter];
            light.m_Position  = lc.m_Position;
            light.m_Direction = lc.m_Direction;
            light.m_Color     = lc.m_Color;
            light.m_Type      = static_cast<int>(lc.m_LightType);
            light.m_Intensity = lc.m_Intensity;
            light.m_Radius    = lc.m_Radius;
            light.m_ConeAngle = lc.m_ConeAngle;
            ++counter;
        }
        m_LightCount = counter;
    }

    size_t LightContainer::GetLightCount() const
    {
        return m_LightCount;
    }

    const std::vector<LightData>& LightContainer::GetLights() const
    {
        return m_Lights;
    }
}
