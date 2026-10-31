#pragma once

#include "GpuPassTiming.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Renderer
{
    enum class GpuTimelineEventKind : uint8_t
    {
        Pass,    // one run of a render-graph pass, its barriers included
        Scope,   // a named scope inside it (IRHICommandList::BeginScope)
        Command, // one timed command inside it
    };

    // One span of the GPU's work in a frame, every time measured from the frame's first timestamp
    // on one clock, so passes, scopes and commands line up: each scope and command lies within its
    // pass (and its scopes).
    struct GpuTimelineEvent
    {
        GpuTimelineEventKind Kind    = GpuTimelineEventKind::Pass;
        const char*          Name    = "";       // the pass's or scope's, a command's kind; lives as long as the process
        const char*          Pass    = "";       // the pass it is in (its own for a pass)
        GpuCommandKind       Command = GpuCommandKind::Draw; // a command's
        uint32_t             Count     = 0;      // a command's, as GpuCommandTiming
        uint32_t             Instances = 0;
        uint32_t             Depth     = 0;      // 0 a pass, then 1 more per scope it is in
        double               StartMs    = 0.0;
        double               DurationMs = 0.0;
    };

    // The frame's timeline in GPU order (each pass, then what it holds, each scope before its
    // contents) from its timestamps: frameStart the command list's first, passTicks one before the
    // first pass then one after each of passNames (passNames.size() + 1), and, when commands were
    // timed, commandTicks one before the first then one after each of commands
    // (commands.size() + 1; both empty otherwise). A pass several views run is one event per run.
    // A command belongs to the pass whose end it does not pass; commands after the last timed pass
    // are left out. Its start is clamped into its pass and its scope; a scope still open at its
    // pass's end closes there. out is rebuilt, its storage reused.
    void BuildGpuTimeline(uint64_t frameStart, std::span<const std::string> passNames, std::span<const uint64_t> passTicks,
                          std::span<const GpuCommandTiming> commands, std::span<const uint64_t> commandTicks,
                          double msPerTick, std::vector<GpuTimelineEvent>& out);
}
