#include "LightContainer.hpp"

#include "Scene/Scene.hpp"
#include "Scene/SceneComponents/LightComponent.hpp"

#include "HedgehogCommon/Common/RendererSettings.hpp"
#include "HedgehogCommon/Common/EngineDebugBreak.hpp"

#include "HedgehogMath/Common.hpp"
#include "HedgehogMath/Vector.hpp"
#include "Logger/Logger.hpp"

#include <cmath>
#include <algorithm>

namespace Context
{
    LightContainer::LightContainer()
    {
        mLights.resize(MAX_LIGHTS_COUNT);
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
                auto& light        = mLights[counter];
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
        mLightCont = counter;
    }

    size_t LightContainer::GetLightCount() const
    {
        return mLightCont;
    }

    const std::vector<Light>& LightContainer::GetLights() const
    {
        return mLights;
    }
}
