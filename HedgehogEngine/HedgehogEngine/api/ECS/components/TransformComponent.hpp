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

        HM::Matrix4x4 m_ObjMatrix = HM::Matrix4x4(); // runtime-only, not serialized

        template<typename V>
        void Visit(V& v)
        {
            v("Position", m_Position);
            v("Rotation", m_Rotation);
            v("Scale",    m_Scale);
        }
    };
}
