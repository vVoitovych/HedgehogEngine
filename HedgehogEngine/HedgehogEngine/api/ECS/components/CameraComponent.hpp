#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include <cstdint>
#include <string>

namespace HedgehogEngine
{
    enum class CameraProjectionType
    {
        Perspective  = 0,
        Orthographic = 1
    };

    // RENDERING.md section 1's "TargetIntent" (Main | Texture(name)) split into two reflected
    // fields: the reflection/serialisation layer only supports plain struct fields, not a
    // tagged union. TargetName is meaningless while TargetMode is Main.
    enum class CameraTargetMode
    {
        Main    = 0,
        Texture = 1
    };

HH_BEGIN_COMPONENT(CameraComponent)
    HH_PROP_NAMED(bool, IsEnabled, "Enabled", true, None)

    static constexpr const char* kProjectionTypeNames[] = { "Perspective", "Orthographic" };
    HH_PROP_NAMED_ENUM(HedgehogEngine::CameraProjectionType, ProjectionType, "ProjectionType",
                       HedgehogEngine::CameraProjectionType::Perspective, kProjectionTypeNames, 2)

    HH_PROP_NAMED_SLIDER(float, Fov,       "Fov",       60.0f, 1.0f, 179.0f)
    HH_PROP_NAMED(float,        OrthoSize, "OrthoSize", 10.0f,       None)
    HH_PROP_NAMED(float,        NearPlane, "NearPlane", 0.1f,        None)
    HH_PROP_NAMED(float,        FarPlane,  "FarPlane",  1000.0f,     None)

    // Bitmask over HedgehogSettings::LayerSettings' 32 layers: which layers this camera
    // renders. Unlike RenderComponent::Layer (an index — an object is in exactly one layer),
    // a camera can see several layers at once, hence a mask rather than an index. Defaults to
    // every layer, so a freshly added camera is immediately useful.
    HH_PROP_NAMED(uint32_t, LayerMask, "LayerMask", 0xFFFFFFFFu, None)

    static constexpr const char* kTargetModeNames[] = { "Main", "Texture" };
    HH_PROP_NAMED_ENUM(HedgehogEngine::CameraTargetMode, TargetMode, "TargetMode",
                       HedgehogEngine::CameraTargetMode::Main, kTargetModeNames, 2)
    HH_PROP_NAMED(std::string, TargetName, "TargetName", std::string{}, None)

    // Which graph asset (RENDERING.md section 6) this camera's view instantiates. Defaults to
    // "game" rather than "scene": the editor's scene and result views are constructed directly
    // by the editor (RENDERING.md section 7), never through a CameraComponent — an entity
    // carrying this component is always somebody's *game* camera.
    HH_PROP_NAMED(std::string, GraphName, "GraphName", std::string{"game"}, None)

    // Tie-break only among views with no render-target dependency between them
    // (RENDERING.md section 3.3) — not a general draw-order control.
    HH_PROP_NAMED(int32_t, Priority, "Priority", 0, None)
HH_END_COMPONENT(CameraComponent)
}
