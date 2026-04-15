#include "LightContainer.hpp"

#include "Scene/Scene.hpp"
#include "Scene/SceneComponents/LightComponent.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"
#include "HedgehogCommon/api/EngineDebugBreak.hpp"

#include "HedgehogMath/api/Common.hpp"
#include "HedgehogMath/api/Vector.hpp"
#include "Logger/api/Logger.hpp"

#include <cmath>
#include <algorithm>

namespace Context
{
    LightContainer::LightContainer()
    {
        m_Lights.resize(MAX_LIGHTS_COUNT);
    }

    void LightContainer::UpdateLights(const Scene::Scene& scene)
    {
        auto lightComponentsCount = scene.GetLightCount();
        if (lightComponentsCount > MAX_LIGHTS_COUNT)
        {
            LOGWARNING("Too many light components. Some of them will not be processed!");
        }
        lightComponentsCount = std::min(lightComponentsCount, static_cast<size_t>(MAX_LIGHTS_COUNT));

        size_t counter = 0;
        for (size_t i = 0; i < lightComponentsCount; ++i)
        {
            const auto& lightComponent = scene.GetLightComponentByIndex(i);
            if (lightComponent.m_Enable)
            {
                auto& light        = m_Lights[counter];
                light.m_Position   = lightComponent.m_Position;
                light.m_Direction  = lightComponent.m_Direction;
                light.m_Color      = lightComponent.m_Color;
                light.m_Data       = {
                    static_cast<float>(lightComponent.m_LightType),
                    lightComponent.m_Intensity,
                    lightComponent.m_Radius,
                    std::cos(HM::ToRadians(lightComponent.m_ConeAngle))
                };
                ++counter;
            }
        }
        m_LightCount = counter;
    }

    size_t LightContainer::GetLightCount() const
    {
        return m_LightCount;
    }

    const std::vector<Light>& LightContainer::GetLights() const
    {
        return m_Lights;
    }
}
