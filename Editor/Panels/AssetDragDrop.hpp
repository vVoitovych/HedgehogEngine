#pragma once

#include "ContentPanel.hpp"

#include "imgui.h"

#include <initializer_list>
#include <optional>
#include <string>

namespace Editor
{
    // Dragging assets out of the Content panel. The payload ("HH_ASSET") carries the asset's
    // virtual path and its type (ContentTypes.hpp), so every drop target can refuse a type it
    // does not take.

    // Makes the last item a drag source for the asset, previewed as its icon and name.
    void DragAssetSource(const std::string& virtualPath, ContentType type, const std::string& name);

    // Makes the last item a drop target for assets of the accepted types: a matching asset
    // highlights it, any other shows a not-allowed cursor. Returns the asset released on it.
    [[nodiscard]] std::optional<ContentOpenRequest> AcceptAssetDrop(std::initializer_list<ContentType> accepted);

    // The coloured tile the Content panel draws for a type; the drag preview reuses it.
    void DrawAssetIcon(ImDrawList& drawList, const ImVec2& min, float size, ContentType type);
}
