#pragma once

#include "HedgehogMath/api/Vector.hpp"
#include "HedgehogMath/api/Matrix.hpp"

namespace HedgehogEngine
{
    class TransformComponent
    {
    public:
        HM::Vector3   m_Position  = HM::Vector3(0.0f, 0.0f, 0.0f);
        HM::Vector3   m_Rotation  = HM::Vector3(0.0f, 0.0f, 0.0f);
        HM::Vector3   m_Scale     = HM::Vector3(1.0f, 1.0f, 1.0f);

        HM::Matrix4x4 m_LocalMatrix = HM::Matrix4x4(); // local-space T·Rx·Ry·Rz·S, not serialised
        HM::Matrix4x4 m_ObjMatrix   = HM::Matrix4x4(); // world-space parent·local, not serialised

        template<typename V>
        void Visit(V& v)
        {
            v("Position", m_Position);
            v("Rotation", m_Rotation);
            v("Scale",    m_Scale);
        }
    };
}
