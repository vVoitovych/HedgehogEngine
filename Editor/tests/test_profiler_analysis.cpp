#include "Tools/Profiler/ProfilerAnalysis.hpp"

#include "doctest/doctest/doctest.h"

#include <string>

using namespace Editor;

namespace
{
    constexpr uint64_t MS = 1000000; // nanoseconds

    // A frame from 0 to 10 ms: on the main thread Frame (0-10) holds Update (1-6), which holds
    // Physics (2-4) and Physics again (4.5-5.5), and Render (6-9); a worker runs Physics (3-7).
    // Zones are listed as the recorder ends them: a child before its parent.
    Profiling::FrameRecord MakeFrame()
    {
        Profiling::FrameRecord frame;
        frame.StartNs = 0;
        frame.EndNs   = 10 * MS;

        Profiling::ThreadRecord main;
        main.ThreadName = "Main";
        main.Zones      = {
            { "Physics", 2 * MS, 2 * MS, 2 },
            { "Physics", 4 * MS + MS / 2, MS, 2 },
            { "Update", 1 * MS, 5 * MS, 1 },
            { "Render", 6 * MS, 3 * MS, 1 },
            { "Frame", 0, 10 * MS, 0 },
        };
        Profiling::ThreadRecord worker;
        worker.ThreadName = "Worker";
        // The same name made in another module: another pointer, the same text.
        static const std::string physics = "Physics";
        worker.Zones = { { physics.c_str(), 3 * MS, 4 * MS, 0 } };

        frame.Threads = { main, worker };
        return frame;
    }

    const ZoneStat* Find(const std::vector<ZoneStat>& stats, const std::string& name)
    {
        for (const ZoneStat& stat : stats)
        {
            if (stat.Name == name)
                return &stat;
        }
        return nullptr;
    }
}

TEST_CASE("Profiler analysis - top zones total and self time by name over every thread, the longest first")
{
    std::vector<ZoneStat> stats;
    ComputeTopZones(MakeFrame(), stats);
    REQUIRE(stats.size() == 4);
    CHECK(stats[0].Name == "Frame");
    CHECK(stats[1].Name == "Physics"); // 2 + 1 + 4 = 7 ms
    CHECK(stats[2].Name == "Update");
    CHECK(stats[3].Name == "Render");

    const ZoneStat* frame   = Find(stats, "Frame");
    const ZoneStat* update  = Find(stats, "Update");
    const ZoneStat* physics = Find(stats, "Physics");
    const ZoneStat* render  = Find(stats, "Render");
    REQUIRE((frame && update && physics && render));
    CHECK(frame->TotalMs == doctest::Approx(10.0));
    CHECK(frame->SelfMs == doctest::Approx(10.0 - 5.0 - 3.0));
    CHECK(update->SelfMs == doctest::Approx(5.0 - 2.0 - 1.0));
    CHECK(physics->TotalMs == doctest::Approx(7.0));
    CHECK(physics->SelfMs == doctest::Approx(7.0));
    CHECK(physics->Calls == 3);
    CHECK(render->SelfMs == doctest::Approx(3.0));

    // An empty frame has none, and the output is reused.
    ComputeTopZones(Profiling::FrameRecord{}, stats);
    CHECK(stats.empty());
}

TEST_CASE("Profiler analysis - the timeline puts each zone on its depth's row over the frame's span")
{
    const Profiling::FrameRecord frame = MakeFrame();
    std::vector<TimelineBar>     bars;
    LayoutTimeline(frame.Threads[0], frame.StartNs, frame.EndNs, 1000.0f, bars);
    REQUIRE(bars.size() == 5);

    // In start order, a parent before its children.
    CHECK(std::string(bars[0].Name) == "Frame");
    CHECK(bars[0].X0 == doctest::Approx(0.0f));
    CHECK(bars[0].X1 == doctest::Approx(1000.0f));
    CHECK(bars[0].Row == 0);
    CHECK(std::string(bars[1].Name) == "Update");
    CHECK(bars[1].X0 == doctest::Approx(100.0f));
    CHECK(bars[1].X1 == doctest::Approx(600.0f));
    CHECK(bars[1].Row == 1);
    CHECK(std::string(bars[2].Name) == "Physics");
    CHECK(bars[2].X0 == doctest::Approx(200.0f));
    CHECK(bars[2].Row == 2);
    CHECK(bars[2].Ms == doctest::Approx(2.0));
    CHECK(std::string(bars[4].Name) == "Render");
    CHECK(bars[4].X0 == doctest::Approx(600.0f));

    // A zone begun before the frame starts at its left edge, and one past its end stops at the right.
    Profiling::ThreadRecord spanning;
    spanning.Zones = { { "Long", 0, 30 * MS, 0 } };
    LayoutTimeline(spanning, 10 * MS, 20 * MS, 500.0f, bars);
    REQUIRE(bars.size() == 1);
    CHECK(bars[0].X0 == doctest::Approx(0.0f));
    CHECK(bars[0].X1 == doctest::Approx(500.0f));

    CHECK(GetFrameMs(frame) == doctest::Approx(10.0));
}

TEST_CASE("GPU commands - named by kind, count and instances, and summed by pass in first-seen order")
{
    using Renderer::GpuCommandKind;
    const std::vector<Renderer::GpuCommandTiming> commands = {
        { "Shadow", GpuCommandKind::Barrier, 1, 1, 0.01 },
        { "Shadow", GpuCommandKind::DrawIndexed, 36, 1, 0.20 },
        { "Forward", GpuCommandKind::DrawIndexed, 1, 2, 0.30 },
        { "Shadow", GpuCommandKind::Draw, 3, 1, 0.05 },
        { "Forward", GpuCommandKind::Mipmaps, 1, 1, 0.10 },
    };

    CHECK(DescribeGpuCommand(commands[0]) == "Barrier 1 transition");
    CHECK(DescribeGpuCommand(commands[1]) == "DrawIndexed 36 indices");
    CHECK(DescribeGpuCommand(commands[2]) == "DrawIndexed 1 index x 2 instances");
    CHECK(DescribeGpuCommand(commands[3]) == "Draw 3 vertices");
    CHECK(DescribeGpuCommand(commands[4]) == "Mipmaps");
    CHECK(DescribeGpuCommand({ "Upload", GpuCommandKind::CopyBuffer, 48, 1, 0.0 }) == "CopyBuffer 48 bytes");

    // A pass name spelt by another pointer is the same pass.
    const std::string shadow = "Shadow";
    std::vector<Renderer::GpuCommandTiming> spelt = commands;
    spelt[3].Pass = shadow.c_str();

    std::vector<PassCommandStat> stats = { { "stale" } };
    SummarizeCommandsByPass(spelt, stats);
    REQUIRE(stats.size() == 2);
    CHECK(stats[0].Pass == "Shadow");
    CHECK(stats[0].Commands == 3);
    CHECK(stats[0].Draws == 2);
    CHECK(stats[0].GpuMs == doctest::Approx(0.26));
    CHECK(stats[1].Pass == "Forward");
    CHECK(stats[1].Commands == 2);
    CHECK(stats[1].Draws == 1);
    CHECK(stats[1].GpuMs == doctest::Approx(0.40));

    SummarizeCommandsByPass({}, stats);
    CHECK(stats.empty());
}
