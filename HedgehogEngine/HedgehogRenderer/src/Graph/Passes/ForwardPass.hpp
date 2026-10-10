#pragma once

#include "HedgehogRenderer/Graph/PassBuilderRegistry.hpp"

namespace Renderer
{
    // Forward: slots color, depth and shadowMap, and the Flag parameter cullBackFaces. Clears color
    // and draws the lit opaque instances into it, depth-tested against the prepass depth without
    // writing it and sampling the shadow atlas.
    [[nodiscard]] PassTypeInfo GetForwardPassType();

    // ForwardTransparent: slots color, depth and shadowMap, as Forward's. Transparent materials'
    // instances, blended back to front over the view's HDR target (after the Skybox, before ToneMap)
    // against the prepass depth, which they do not write; lit as Forward lights. Records nothing
    // without any.
    [[nodiscard]] PassTypeInfo GetForwardTransparentPassType();
}
