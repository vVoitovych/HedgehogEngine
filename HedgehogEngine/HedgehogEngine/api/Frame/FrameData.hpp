#pragma once

#include "HedgehogMath/api/Matrix.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include <optional>
#include <vector>
#include <cstdint>

namespace FD
{
    struct DrawObject
    {
        uint64_t      m_MeshIndex;
        HM::Matrix4x4 m_Transform;
    };

    struct DrawNode
    {
        uint64_t                m_MaterialIndex;
        std::vector<DrawObject> m_Objects;
    };

    using DrawBucket = std::vector<DrawNode>;

    struct DrawList
    {
        DrawBucket m_Opaque;
        DrawBucket m_Cutoff;
        DrawBucket m_Transparent;
    };

    struct LightData
    {
        HM::Vector3 m_Position;
        HM::Vector3 m_Direction;
        HM::Vector3 m_Color;
        float       m_Intensity = 1.0f;
        float       m_Radius    = 1.0f;
        float       m_ConeAngle = 0.0f;
        int         m_Type      = 0;  // 0=directional, 1=point, 2=spot
    };

    struct CameraData
    {
        HM::Matrix4x4 m_View;
        HM::Matrix4x4 m_Proj;
        HM::Vector3   m_Position;
        float         m_Near = 0.1f;
        float         m_Far  = 1000.0f;
    };

    struct FrameData
    {
        DrawList                   m_DrawList;
        std::vector<LightData>     m_Lights;
        CameraData                 m_Camera;
        float                      m_DeltaTime = 0.0f;
        std::optional<HM::Vector3> m_ShadowLightDirection;
    };
}
