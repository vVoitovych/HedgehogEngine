#pragma once

#include "Profiler/api/Profiler.hpp"

#include "HedgehogRenderer/Graph/GpuPassTiming.hpp"

#include <span>
#include <string>

// A profiler capture as Chrome's trace event JSON, which Perfetto (ui.perfetto.dev) and
// chrome://tracing open. ImGui-free, so EditorTest covers it.
namespace Editor
{
    // One frame's GPU passes, laid one after another from StartNs: the GPU's own clock is not the
    // CPU's, so a pass shows how long it took and in what order, not exactly when.
    struct TraceGpuFrame
    {
        uint64_t                                 StartNs = 0;
        std::span<const Renderer::GpuPassTiming> Passes;
    };

    // Process 1 ("CPU") holds a track per thread (its ThreadIndex, named by a metadata event) of
    // complete events ("ph": "X") for every zone; process 2 ("GPU") one track of the passes, when
    // there are GPU frames. Times are microseconds, three decimals, from the earliest time given.
    [[nodiscard]] std::string WriteChromeTrace(std::span<const Profiling::FrameRecord* const> frames,
                                               std::span<const TraceGpuFrame>                gpuFrames);
}
