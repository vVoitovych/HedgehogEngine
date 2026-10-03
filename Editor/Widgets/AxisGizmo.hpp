#pragma once

#include "HedgehogMath/api/Matrix.hpp"

#include <array>

namespace Editor
{
    // A world axis as the camera sees it: its screen direction (x right, y down, unit length when
    // the axis lies in the view plane, shorter as it turns towards or away from the camera) and its
    // depth (positive towards the viewer).
    struct ProjectedAxis
    {
        float X     = 0.0f;
        float Y     = 0.0f;
        float Depth = 0.0f;
    };

    // The world X, Y and Z axes, in that order, as the view matrix turns them. The view matrix is a
    // look-at matrix: column k of its rotation is world axis k in view space (x right, y up, -z forward).
    [[nodiscard]] std::array<ProjectedAxis, 3> ProjectWorldAxes(const HM::Matrix4x4& view);
}
