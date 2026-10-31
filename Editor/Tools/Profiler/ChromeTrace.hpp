#pragma once

#include "Profiler/api/Profiler.hpp"

#include <span>
#include <string>

// A profiler capture as Chrome's trace event JSON, which Perfetto (ui.perfetto.dev) and
// chrome://tracing open. ImGui-free, so EditorTest covers it.
namespace Editor
{
    // Process 1 ("CPU") holds a track per thread (its ThreadIndex, named by a metadata event) of
    // complete events ("ph": "X") for every zone; process 2 ("GPU"), when gpuThreads holds any (each
    // frame's MakeGpuThread), one track ("GPU") of their zones, nested as they are. Times are
    // microseconds, three decimals, from the earliest time given.
    [[nodiscard]] std::string WriteChromeTrace(std::span<const Profiling::FrameRecord* const> frames,
                                               std::span<const Profiling::ThreadRecord* const> gpuThreads);
}
