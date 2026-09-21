#pragma once

#include "HedgehogMath/api/Vector.hpp"
#include "HedgehogMath/api/Matrix.hpp"
#include "HedgehogMath/api/AABB.hpp"

#include <cstdint>
#include <string>
#include <vector>

// RenderScene is the boundary RENDERING.md section 3.1 describes: SceneExtractor is the only
// code that reads the ECS, and every type below is plain data — no RHI types, no ECS types.
// Everything downstream (view building, culling, the graph) reads only this.
namespace HX
{
    // Mirrors HedgehogEngine::CameraProjectionType, kept as an independent copy rather than an
    // include of CameraComponent.hpp so this header never pulls in the ECS/reflection headers.
    enum class CameraProjectionType
    {
        Perspective  = 0,
        Orthographic = 1
    };

    // Mirrors HedgehogEngine::CameraTargetMode. See CameraProjectionType above.
    enum class CameraTargetMode
    {
        Main    = 0,
        Texture = 1
    };

    // Mirrors HedgehogEngine::LightType. See CameraProjectionType above.
    enum class LightType
    {
        Directional = 0,
        Point       = 1,
        Spot        = 2
    };

    struct RenderInstance
    {
        HM::Matrix4x4 WorldMatrix;

        // Precomputed so culling never touches mesh data (RENDERING.md section 3.1). Today this
        // is a local unit-cube AABB transformed by WorldMatrix — a placeholder, since no module
        // yet computes real per-mesh local bounds. Replace the source, not the call sites, once
        // one does.
        HM::AABB WorldBounds;

        uint64_t MeshIndex     = 0;
        uint64_t MaterialIndex = 0;

        // Index into HedgehogSettings::LayerSettings — RenderComponent::Layer, passed through.
        uint32_t Layer = 0;

        // The originating ECS::Entity. Used only by editor picking (F7) — never by culling,
        // sorting, or draw submission.
        uint64_t SourceId = 0;
    };

    struct RenderLight
    {
        LightType Type = LightType::Directional;

        HM::Vector3 Position  = HM::Vector3(0.0f, 0.0f, 0.0f);
        HM::Vector3 Direction = HM::Vector3(0.0f, 0.0f, 0.0f);
        HM::Vector3 Color     = HM::Vector3(1.0f, 1.0f, 1.0f);

        float Intensity  = 1.0f;
        float Radius     = 1.0f;
        float ConeAngle  = 1.0f;
        bool  CastShadows = false;

        uint64_t SourceId = 0;
    };

    struct RenderCamera
    {
        HM::Matrix4x4 WorldMatrix;

        CameraProjectionType ProjectionType = CameraProjectionType::Perspective;
        float Fov       = 60.0f;
        float OrthoSize = 10.0f;
        float NearPlane = 0.1f;
        float FarPlane  = 1000.0f;

        // Bitmask over HedgehogSettings::LayerSettings' 32 layers.
        uint32_t LayerMask = 0xFFFFFFFFu;

        CameraTargetMode TargetMode = CameraTargetMode::Main;
        std::string      TargetName;
        std::string      GraphName = "game";

        int32_t Priority = 0;

        uint64_t SourceId = 0;
    };

    // Reused across frames: SceneExtractor::Extract calls Clear() (vector::clear(), not
    // reassignment) so steady-state extraction into an already-sized RenderScene allocates
    // nothing.
    struct RenderScene
    {
        std::vector<RenderInstance> Instances;
        std::vector<RenderLight>    Lights;
        std::vector<RenderCamera>   Cameras;

        void Clear()
        {
            Instances.clear();
            Lights.clear();
            Cameras.clear();
        }
    };
}
