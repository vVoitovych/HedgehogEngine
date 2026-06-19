#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include "HedgehogMath/api/Vector.hpp"

namespace HedgehogEngine
{
    enum class LightType
    {
        DirectionLight = 0,
        PointLight     = 1,
        SpotLight      = 2
    };

HH_BEGIN_COMPONENT(LightComponent)
    HH_PROP_NAMED(bool, m_Enable, "LightEnabled", true, None)

    static constexpr const char* kLightTypeNames[] = { "Direction Light", "Point Light", "Spot Light" };
    HH_PROP_NAMED_ENUM(LightType, m_LightType, "LightType", LightType::DirectionLight, kLightTypeNames, 3)

    HH_PROP_NAMED(HM::Vector3, m_Color,      "LightColor",     HM::Vector3(1.0f, 1.0f, 1.0f), IsColor)
    HH_PROP_NAMED_SLIDER(float, m_Intensity,  "LightIntensity", 1.0f, 0.0f, 3.0f)
    HH_PROP_NAMED_SLIDER(float, m_Radius,     "LightRadius",    1.0f, 0.0f, 100.0f)
    HH_PROP_NAMED_SLIDER(float, m_ConeAngle,  "LightConeAngle", 1.0f, 0.1f, 179.9f)
    HH_PROP_NAMED(bool, m_CastShadows, "CastShadows", false, None)

    HM::Vector3 m_Position  = HM::Vector3(0.0f, 0.0f, 0.0f); // runtime: filled by LightSystem
    HM::Vector3 m_Direction = HM::Vector3(0.0f, 0.0f, 0.0f); // runtime: filled by LightSystem
HH_END_COMPONENT(LightComponent)
}
