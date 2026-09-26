#pragma once

#include "HedgehogRenderer/Graph/PassBuilderRegistry.hpp"

namespace Renderer
{
    // Shadow: slot shadowMap, the cascaded shadow atlas, which it clears and fills with one tile per
    // cascade. Declared by the shared phase (SharedPhase.hpp), never by a view graph.
    [[nodiscard]] PassTypeInfo GetShadowPassType();
}
