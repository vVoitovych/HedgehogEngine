#pragma once

#include "imgui.h"

// Drawing the editor's line icons (Panels/EditorIcons). The pictures are white, so the tint is the
// colour they show in; pass colours from the style or through Theme::Resolve.
namespace Editor
{
    // The size an icon is drawn at, in pixels.
    inline constexpr float ICON_SIZE_SMALL = 16.0f;

    // Draws icon in the square at min; a missing icon (nullptr) draws nothing.
    void DrawIcon(ImDrawList& drawList, void* icon, ImVec2 min, float size, ImU32 tint);

    // A button showing icon, framed like any button. A missing icon leaves an empty button of the
    // same size, so the layout does not move.
    bool IconButton(const char* strId, void* icon, float size, const ImVec4& tint);
}
