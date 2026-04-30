#pragma once

#include "HedgehogMath/api/Vector.hpp"

namespace HedgehogEngine
{
    enum class LightType
    {
        DirectionLight = 0,
        PointLight     = 1,
        SpotLight      = 2
    };

    class LightComponent
    {
    public:
        bool        m_Enable    = true;
        LightType   m_LightType = LightType::DirectionLight;
        HM::Vector3 m_Color     = { 1.0f, 1.0f, 1.0f };
        float       m_Intensity = 1.0f;
        float       m_Radius    = 1.0f;
        float       m_ConeAngle = 1.0f;

        bool m_CastShadows = false;

        HM::Vector3 m_Position  = HM::Vector3(0.0f, 0.0f, 0.0f); // runtime: filled by LightSystem
        HM::Vector3 m_Direction = HM::Vector3(0.0f, 0.0f, 0.0f); // runtime: filled by LightSystem

        template<typename V>
        void Visit(V& v)
        {
            v("LightEnabled",   m_Enable);
            v("LightType",      m_LightType);
            v("LightColor",     m_Color);
            v("LightIntensity", m_Intensity);
            v("LightRadius",    m_Radius);
            v("LightConeAngle", m_ConeAngle);
        }
    };
}
