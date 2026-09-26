#pragma once

#include "HedgehogRenderer/Graph/PassBuilderRegistry.hpp"

namespace Renderer
{
    // Gizmo: slots color and depth. Draws the overlay instances' world bounds as wireframe boxes
    // over what the view has drawn, depth-tested without writing depth. Records nothing when the
    // view has no overlay instances.
    [[nodiscard]] PassTypeInfo GetGizmoPassType();
}
