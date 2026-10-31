#include "HedgehogRenderer/Graph/GpuTimeline.hpp"

#include "doctest/doctest/doctest.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace Renderer;

namespace
{
    // Each event as "<depth> <name> <start>+<duration>".
    std::vector<std::string> Describe(const std::vector<GpuTimelineEvent>& events)
    {
        std::vector<std::string> out;
        for (const GpuTimelineEvent& event : events)
        {
            char text[128];
            std::snprintf(text, sizeof(text), "%u %s %.0f+%.0f", event.Depth, event.Name, event.StartMs, event.DurationMs);
            out.push_back(text);
        }
        return out;
    }
}

TEST_CASE("GPU timeline - passes, scopes and commands on one clock, a pass run twice twice")
{
    // One tick a millisecond, the frame starting at tick 100. Shadow runs twice (two views).
    const std::vector<std::string> passes    = { "Shadow", "Forward", "Shadow" };
    const std::vector<uint64_t>    passTicks = { 110, 130, 150, 160 };
    using K = GpuCommandKind;
    const std::vector<GpuCommandTiming> commands = {
        { "Shadow", K::Barrier, 1, 1 },
        { "Shadow", K::ScopeBegin, 0, 0, 0.0, "Rigid" },
        { "Shadow", K::DrawIndexed, 36, 2 },
        { "Shadow", K::ScopeEnd, 0, 0 },
        { "Shadow", K::ScopeBegin, 0, 0, 0.0, "Open" }, // never closed: closes with its pass
        { "Forward", K::DrawIndexed, 12, 1 },           // starts before its pass: clamped into it
        { "Forward", K::Draw, 3, 1 },
        { "Shadow", K::Draw, 3, 1 },
        { "Present", K::Blit, 1, 1 },                   // after the last timed pass: left out
    };
    const std::vector<uint64_t> commandTicks = { 110, 112, 114, 120, 126, 128, 140, 148, 158, 170 };

    std::vector<GpuTimelineEvent> timeline;
    BuildGpuTimeline(100, passes, passTicks, commands, commandTicks, 1.0, timeline);
    CHECK(Describe(timeline) == std::vector<std::string>{
        "0 Shadow 10+20", "1 Barrier 10+2", "1 Rigid 14+12", "2 DrawIndexed 14+6", "1 Open 28+2",
        "0 Forward 30+20", "1 DrawIndexed 30+10", "1 Draw 40+8",
        "0 Shadow 50+10", "1 Draw 50+8" });
    CHECK(timeline[3].Kind == GpuTimelineEventKind::Command);
    CHECK(timeline[3].Count == 36);
    CHECK(timeline[3].Instances == 2);
    CHECK(timeline[2].Kind == GpuTimelineEventKind::Scope);
    CHECK(std::string(timeline[9].Pass) == "Shadow");
    // The two Shadow runs share one interned name.
    CHECK(timeline[0].Name == timeline[8].Name);

    // Every event lies within the one it is nested in.
    std::vector<const GpuTimelineEvent*> stack;
    for (const GpuTimelineEvent& event : timeline)
    {
        stack.resize(event.Depth);
        if (!stack.empty())
        {
            CHECK(event.StartMs >= stack.back()->StartMs);
            CHECK(event.StartMs + event.DurationMs <= stack.back()->StartMs + stack.back()->DurationMs);
        }
        stack.push_back(&event);
    }

    // Without timed commands, the passes alone; built again into the same storage.
    BuildGpuTimeline(100, passes, passTicks, {}, {}, 0.5, timeline);
    CHECK(Describe(timeline) == std::vector<std::string>{ "0 Shadow 5+10", "0 Forward 15+10", "0 Shadow 25+5" });

    // Ticks that do not cover the passes give nothing.
    BuildGpuTimeline(100, passes, std::vector<uint64_t>{ 110, 130 }, {}, {}, 1.0, timeline);
    CHECK(timeline.empty());
}
