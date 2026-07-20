#pragma once

#include "HedgehogMath/api/Matrix.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include <optional>
#include <vector>
#include <cstdint>

namespace HedgehogEngine
{
    struct DrawObject
    {
        uint64_t      MeshIndex;
        HM::Matrix4x4 Transform;
    };

    struct DrawNode
    {
        uint64_t                MaterialIndex;
        std::vector<DrawObject> Objects;
    };

    using DrawBucket = std::vector<DrawNode>;

    struct DrawList
    {
        DrawBucket Opaque;
        DrawBucket Cutoff;
        DrawBucket Transparent;
    };

    struct LightData
    {
        HM::Vector3 Position;
        HM::Vector3 Direction;
        HM::Vector3 Color;
        float       Intensity = 1.0f;
        float       Radius    = 1.0f;
        float       ConeAngle = 0.0f;
        int         Type      = 0;  // 0=directional, 1=point, 2=spot
    };

    struct CameraData
    {
        HM::Matrix4x4 View;
        HM::Matrix4x4 Proj;
        HM::Vector3   Position;
        float         Near = 0.1f;
        float         Far  = 1000.0f;
    };

    struct FrameData
    {
        HedgehogEngine::DrawList   DrawList;
        std::vector<LightData>     Lights;
        CameraData                 Camera;
        float                      DeltaTime = 0.0f;
        std::optional<HM::Vector3> ShadowLightDirection;
    };
}
