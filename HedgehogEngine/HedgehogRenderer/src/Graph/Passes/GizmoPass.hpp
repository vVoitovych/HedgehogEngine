#pragma once

#include "HedgehogRenderer/Graph/PassBuilderRegistry.hpp"

namespace Renderer
{
    // Gizmo: slots color and depth. Draws the frame's debug lines (RenderScene::DebugLines: the
    // collider wireframes) over what the view has drawn, depth-tested without writing depth. Records
    // nothing without lines. The selection is outlined by the SelectionOutline pass.
    [[nodiscard]] PassTypeInfo GetGizmoPassType();
}
