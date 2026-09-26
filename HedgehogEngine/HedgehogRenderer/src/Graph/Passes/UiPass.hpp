#pragma once

#include "HedgehogRenderer/Graph/PassBuilderRegistry.hpp"

namespace Renderer
{
    // Ui: slot target. Runs the frame's UiCallback into target, or clears it without one, after
    // sampling the render targets the view reads (GraphFrameData::UiSampledTargets).
    [[nodiscard]] PassTypeInfo GetUiPassType();
}
