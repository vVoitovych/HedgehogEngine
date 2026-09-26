#pragma once

#include "HedgehogRenderer/Graph/PassBuilderRegistry.hpp"

namespace Renderer
{
    // Forward: slots color, depth and shadowMap, and the Flag parameter cullBackFaces. Clears color
    // and draws the lit opaque instances into it, depth-tested against the prepass depth without
    // writing it and sampling the shadow atlas.
    [[nodiscard]] PassTypeInfo GetForwardPassType();
}
