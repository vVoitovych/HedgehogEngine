#pragma once

#include "Profiler/api/Profiler.hpp"

#include "HedgehogRenderer/Graph/GpuPassTiming.hpp"

#include <span>
#include <string>

// A profiler capture as Chrome's trace event JSON, which Perfetto (ui.perfetto.dev) and
// chrome://tracing open. ImGui-free, so EditorTest covers it.
namespace Editor
{
    // One frame's GPU passes, and its timed commands when it has them, each laid one after another
    // from StartNs: the GPU's own clock is not the CPU's, so a pass or command shows how long it took
    // and in what order, not exactly when.
    struct TraceGpuFrame
    {
        uint64_t                                    StartNs = 0;
        std::span<const Renderer::GpuPassTiming>    Passes;
        std::span<const Renderer::GpuCommandTiming> Commands;
    };

    // Process 1 ("CPU") holds a track per thread (its ThreadIndex, named by a metadata event) of
    // complete events ("ph": "X") for every zone; process 2 ("GPU") one track of the passes, when
    // there are GPU frames, and a second (tid 1, "GPU commands") of the commands, named as
    // DescribeGpuCommand names them with their pass in args, when any frame has them. Times are microseconds, three decimals, from the earliest time given.
    [[nodiscard]] std::string WriteChromeTrace(std::span<const Profiling::FrameRecord* const> frames,
                                               std::span<const TraceGpuFrame>                gpuFrames);
}
