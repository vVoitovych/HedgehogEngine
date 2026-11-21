#pragma once

#include "HedgehogAnimation/api/AnimatorController.hpp"

#include <array>
#include <optional>
#include <vector>

namespace Editor
{
    // A 2D blend tree's children as dots in a square fitted around them (the origin's axes drawn
    // faintly), and, when given, the parameters' present point as a ring (during Play).
    void DrawBlendSpacePreview(const std::vector<HedgehogAnimation::BlendChild>& children,
                               std::optional<std::array<float, 2>> point, float size);
}
