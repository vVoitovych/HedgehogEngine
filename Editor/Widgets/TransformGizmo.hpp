#pragma once

#include "TransformGizmoMath.hpp"

#include "HedgehogMath/api/Matrix.hpp"

#include <optional>

struct ImVec2;

namespace Editor
{
    // The Scene view's camera as the gizmo needs it: the editor camera's view matrix and its
    // perspective (vertical field of view in radians).
    struct TransformGizmoCamera
    {
        HM::Matrix4x4 View;
        float         FovRadians = 0.0f;
        float         NearPlane  = 0.1f;
        float         FarPlane   = 1000.0f;
    };

    struct TransformGizmoResult
    {
        // The world matrix after this frame's drag; empty when nothing was dragged.
        std::optional<HM::Matrix4x4> World;
        // The pointer is over a handle or drags one: the camera and picking leave the pointer alone.
        bool Active = false;
    };

    // Calls ImGuizmo's BeginFrame; once per ImGui frame, before any gizmo is drawn.
    void BeginTransformGizmoFrame();

    // ImGuizmo's handles for the tool over the image's rectangle (window coordinates), drawn into the
    // current window, for an object whose world matrix is world.
    [[nodiscard]] TransformGizmoResult DrawTransformGizmo(const TransformGizmoCamera& camera, const ImVec2& imageMin,
                                                          const ImVec2& imageSize, TransformTool tool,
                                                          TransformSpace space, const HM::Matrix4x4& world);
}
