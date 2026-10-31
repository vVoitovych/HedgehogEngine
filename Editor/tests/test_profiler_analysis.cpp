#include "Tools/Profiler/ProfilerAnalysis.hpp"

#include "doctest/doctest/doctest.h"

#include <string>

using namespace Editor;

namespace
{
    constexpr uint64_t MS = 1000000; // nanoseconds

    using Kind = Renderer::GpuTimelineEventKind;

    Renderer::GpuTimelineEvent Span(Kind kind, const char* name, uint32_t depth, double startMs, double ms)
    {
        Renderer::GpuTimelineEvent event;
        event.Kind       = kind;
        event.Name       = name;
        event.Depth      = depth;
        event.StartMs    = startMs;
        event.DurationMs = ms;
        return event;
    }

    Renderer::GpuTimelineEvent Command(Renderer::GpuCommandKind kind, uint32_t count, uint32_t instances,
                                       uint32_t depth = 1, double startMs = 0.0, double ms = 0.0)
    {
        Renderer::GpuTimelineEvent event = Span(Kind::Command, Renderer::GetGpuCommandKindName(kind), depth, startMs, ms);
        event.Command   = kind;
        event.Count     = count;
        event.Instances = instances;
        return event;
    }

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

TEST_CASE("GPU timeline - events named by DescribeGpuEvent and made the GPU thread")
{
    using Renderer::GpuCommandKind;
    const std::vector<Renderer::GpuTimelineEvent> events = {
        Span(Kind::Pass, "Shadow", 0, 0.0, 2.0),
        Span(Kind::Scope, "Rigid", 1, 0.5, 1.0),
        Command(GpuCommandKind::DrawIndexed, 36, 1, 2, 0.5, 0.75),
        Command(GpuCommandKind::Barrier, 1, 1, 1, 1.5, 0.25),
    };
    CHECK(DescribeGpuEvent(events[0]) == "Shadow");
    CHECK(DescribeGpuEvent(events[1]) == "Rigid");
    CHECK(DescribeGpuEvent(events[2]) == "DrawIndexed 36 indices");
    CHECK(DescribeGpuEvent(events[3]) == "Barrier 1 transition");
    CHECK(DescribeGpuEvent(Command(GpuCommandKind::DrawIndexed, 1, 2)) == "DrawIndexed 1 index x 2 instances");
    CHECK(DescribeGpuEvent(Command(GpuCommandKind::Draw, 3, 1)) == "Draw 3 vertices");
    CHECK(DescribeGpuEvent(Command(GpuCommandKind::Mipmaps, 1, 1)) == "Mipmaps");
    CHECK(DescribeGpuEvent(Command(GpuCommandKind::CopyBuffer, 48, 1)) == "CopyBuffer 48 bytes");

    // In GPU order, each from the frame's start, a command named by its description (interned).
    Profiling::ThreadRecord thread;
    MakeGpuThread(events, 10 * MS, thread);
    CHECK(thread.ThreadIndex == GPU_THREAD_INDEX);
    CHECK(thread.ThreadName == "GPU");
    REQUIRE(thread.Zones.size() == 4);
    CHECK(std::string(thread.Zones[0].Name) == "Shadow");
    CHECK(thread.Zones[0].StartNs == 10 * MS);
    CHECK(thread.Zones[0].DurationNs == 2 * MS);
    CHECK(thread.Zones[1].Depth == 1);
    CHECK(thread.Zones[2].Name == Profiling::InternName("DrawIndexed 36 indices"));
    CHECK(thread.Zones[2].StartNs == 10 * MS + MS / 2);
    CHECK(thread.Zones[2].DurationNs == 3 * MS / 4);
    CHECK(thread.Zones[2].Depth == 2);
    CHECK(thread.Zones[3].StartNs == 11 * MS + MS / 2);

    // Made again into the same thread: only the new events.
    MakeGpuThread({}, 0, thread);
    CHECK(thread.Zones.empty());
}

TEST_CASE("Zone categories - names map to their category, others to none")
{
    CHECK(CategorizeZone("Render") == ZoneCategory::Rendering);
    CHECK(CategorizeZone("RenderFrame") == ZoneCategory::Rendering);
    CHECK(CategorizeZone("SyncResources") == ZoneCategory::Rendering);
    CHECK(CategorizeZone("Extract: scene") == ZoneCategory::Rendering);
    CHECK(CategorizeZone("Scripts: OnUpdate") == ZoneCategory::Scripts);
    CHECK(CategorizeZone("ScriptSystem") == ZoneCategory::Scripts);
    CHECK(CategorizeZone("Physics: step") == ZoneCategory::Physics);
    CHECK(CategorizeZone("PhysicsSystem") == ZoneCategory::Physics);
    CHECK(CategorizeZone("Phase: Animation") == ZoneCategory::Animation);
    CHECK(CategorizeZone("AnimationSystem") == ZoneCategory::Animation);
    CHECK(CategorizeZone("Editor UI") == ZoneCategory::UI);
    CHECK(CategorizeZone("Outside windows") == ZoneCategory::UI);
    CHECK(CategorizeZone("UiSystem") == ZoneCategory::UI);
    CHECK(CategorizeZone("Wait for the GPU") == ZoneCategory::WaitingForGpu);

    CHECK_FALSE(CategorizeZone("Frame").has_value());
    CHECK_FALSE(CategorizeZone("Forward").has_value());     // a pass belongs to its parent's
    CHECK_FALSE(CategorizeZone("Phase: Transform").has_value());
    CHECK_FALSE(CategorizeZone("Renderer").has_value());    // whole names only
    CHECK_FALSE(CategorizeZone("Extract:scene").has_value()); // prefixes with their space

    for (size_t i = 0; i < ZONE_CATEGORY_COUNT; ++i)
        CHECK(std::string(GetZoneCategoryName(static_cast<ZoneCategory>(i))) != "?");
    CHECK(GetZoneCategoryColor(ZoneCategory::Rendering) != GetZoneCategoryColor(ZoneCategory::Scripts));
}

TEST_CASE("Zone categories - a thread's self times by category, inherited from the parent, sum to its top level")
{
    // Frame 10 ms: Input 1 ms (uncategorized under Frame: Others), UpdateContext 4 ms holding
    // Scripts: OnUpdate 1.5 ms (with a 0.5 ms TransformSystem inside, inheriting Scripts) and
    // Phase: Animation 1 ms, Render 4 ms holding RenderFrame 3 ms, which holds Forward 1 ms
    // (inherits Rendering) and Wait for the GPU 0.5 ms; Frame's own 1 ms is Others.
    Profiling::ThreadRecord thread;
    thread.Zones = {
        { "Frame", 0, 10 * MS, 0 },
        { "Input", 0, 1 * MS, 1 },
        { "UpdateContext", 1 * MS, 4 * MS, 1 },
        { "Scripts: OnUpdate", 1 * MS, 3 * MS / 2, 2 },
        { "TransformSystem", 1 * MS, MS / 2, 3 },
        { "Phase: Animation", 3 * MS, 1 * MS, 2 },
        { "Render", 5 * MS, 4 * MS, 1 },
        { "RenderFrame", 5 * MS, 3 * MS, 2 },
        { "Forward", 5 * MS, 1 * MS, 3 },
        { "Wait for the GPU", 7 * MS, MS / 2, 3 },
    };

    const CategoryTimes times = ComputeCategoryTimes(thread);
    const auto at = [&](ZoneCategory category) { return times[static_cast<size_t>(category)]; };
    CHECK(at(ZoneCategory::Others) == doctest::Approx(1.0 + 1.0 + 1.5)); // Frame, Input, UpdateContext's own
    CHECK(at(ZoneCategory::Scripts) == doctest::Approx(1.5));
    CHECK(at(ZoneCategory::Animation) == doctest::Approx(1.0));
    CHECK(at(ZoneCategory::Rendering) == doctest::Approx(3.5));
    CHECK(at(ZoneCategory::WaitingForGpu) == doctest::Approx(0.5));
    CHECK(at(ZoneCategory::Physics) == doctest::Approx(0.0));

    double sum = 0.0;
    for (const double ms : times)
        sum += ms;
    CHECK(sum == doctest::Approx(10.0));

    const CategoryTimes empty = ComputeCategoryTimes(Profiling::ThreadRecord{});
    for (const double ms : empty)
        CHECK(ms == 0.0);
}

TEST_CASE("Rendering counters - draws, triangles, vertices, barriers and copies of a frame's commands")
{
    using Renderer::GpuCommandKind;
    const std::vector<Renderer::GpuTimelineEvent> commands = {
        Span(Kind::Pass, "Shadow", 0, 0.0, 1.0), // passes and scopes count as nothing
        Command(GpuCommandKind::Barrier, 2, 1),
        Command(GpuCommandKind::DrawIndexed, 36, 1),
        Span(Kind::Scope, "Instanced", 1, 0.0, 0.0),
        Command(GpuCommandKind::DrawIndexed, 36, 4, 2), // instanced: 12 triangles x 4
        Command(GpuCommandKind::Draw, 3, 1),
        Command(GpuCommandKind::CopyBuffer, 64, 1),
        Command(GpuCommandKind::Mipmaps, 1, 1),
        Command(GpuCommandKind::Blit, 1, 1),
    };
    const RenderingCounters counts = CountRendering(commands);
    const auto at = [&](RenderingCounter counter) { return counts[static_cast<size_t>(counter)]; };
    CHECK(at(RenderingCounter::DrawCalls) == 3);
    CHECK(at(RenderingCounter::Commands) == 7);
    CHECK(at(RenderingCounter::Triangles) == 12 + 48 + 1);
    CHECK(at(RenderingCounter::Vertices) == 36 + 144 + 3);
    CHECK(at(RenderingCounter::Barriers) == 1);
    CHECK(at(RenderingCounter::Copies) == 3);

    const RenderingCounters none = CountRendering({});
    for (const uint64_t count : none)
        CHECK(count == 0);
    for (size_t i = 0; i < RENDERING_COUNTER_COUNT; ++i)
        CHECK(std::string(GetRenderingCounterName(static_cast<RenderingCounter>(i))) != "?");

    CHECK(FormatCount(0) == "0");
    CHECK(FormatCount(9999) == "9999");
    CHECK(FormatCount(12345) == "12.3k");
    CHECK(FormatCount(2500000) == "2.5M");
}

