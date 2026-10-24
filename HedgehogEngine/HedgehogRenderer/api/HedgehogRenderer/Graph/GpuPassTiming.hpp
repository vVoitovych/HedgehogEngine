#pragma once

#include <string>

namespace Renderer
{
    // How long the GPU spent on one render-graph pass (its barriers and its commands), from GPU
    // timestamps; a pass several views run is summed over them.
    struct GpuPassTiming
    {
        std::string Name;
        double      GpuMilliseconds = 0.0;
    };
}
