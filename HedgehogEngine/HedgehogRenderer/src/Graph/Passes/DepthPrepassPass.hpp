#pragma once

#include "HedgehogRenderer/Graph/PassBuilderRegistry.hpp"

namespace Renderer
{
    // DepthPrepass: slot depth, which it clears and fills with the view's opaque instances.
    [[nodiscard]] PassTypeInfo GetDepthPrepassPassType();
}
