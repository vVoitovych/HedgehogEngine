#pragma once

#include "imgui.h"

#include <cstddef>

namespace HM
{
    class Matrix4x4;
}

namespace Editor
{
    // Draws the world axes as the view matrix turns them in the top-right corner of the image at
    // imageMin, with "Persp" under them. Read-only: it takes no input.
    void DrawAxisGizmo(ImDrawList& drawList, ImVec2 imageMin, ImVec2 imageSize, const HM::Matrix4x4& view);

    // Draws "<count> render graph passes", muted, in the bottom-left corner of the image.
    void DrawPassCount(ImDrawList& drawList, ImVec2 imageMin, ImVec2 imageSize, size_t passCount);
}
