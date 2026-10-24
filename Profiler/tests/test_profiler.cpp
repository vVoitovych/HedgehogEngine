#include "Profiler/api/Profiler.hpp"

#include "doctest/doctest/doctest.h"

#include <chrono>
#include <cstring>
#include <string>
#include <thread>

using namespace Profiling;

namespace
{
    // A recorder enabled from an empty ring, disabled and cleared again afterwards.
    struct Recording
    {
        Recording()
        {
            SetFrameHistory(DEFAULT_FRAME_HISTORY);
            Clear();
            MarkFrame(); // starts a frame of this test's own
            SetEnabled(true);
        }

        ~Recording()
        {
            SetEnabled(false);
            Clear();
        }
    };

    const ThreadRecord* FindThread(const FrameRecord& frame, const std::string& name)
    {
        for (const ThreadRecord& thread : frame.Threads)
        {
            if (thread.ThreadName == name)
                return &thread;
        }
        return nullptr;
    }

    bool Named(const ZoneRecord& zone, const char* name)
    {
        return std::strcmp(zone.Name, name) == 0;
    }
}

TEST_CASE("Profiler - nested zones end child first, with their depth and times inside their parent's")
{
    Recording recording;
    SetCurrentThreadName("Main");
    {
        HH_PROFILE_ZONE("Outer");
        {
            HH_PROFILE_ZONE("Inner");
            {
                HH_PROFILE_ZONE("Innermost");
            }
        }
        HH_PROFILE_ZONE("Second");
    }
    MarkFrame();

    REQUIRE(GetFrameCount() == 1);
    const FrameRecord& frame = GetFrame(0);
    const ThreadRecord* main = FindThread(frame, "Main");
    REQUIRE(main != nullptr);
    REQUIRE(main->Zones.size() == 4);
    CHECK(Named(main->Zones[0], "Innermost"));
    CHECK(main->Zones[0].Depth == 2);
    CHECK(Named(main->Zones[1], "Inner"));
    CHECK(main->Zones[1].Depth == 1);
    CHECK(Named(main->Zones[2], "Second"));
    CHECK(main->Zones[2].Depth == 1);
    CHECK(Named(main->Zones[3], "Outer"));
    CHECK(main->Zones[3].Depth == 0);

    const ZoneRecord& outer = main->Zones[3];
    for (const ZoneRecord& zone : main->Zones)
    {
        CHECK(zone.StartNs >= outer.StartNs);
        CHECK(zone.StartNs + zone.DurationNs <= outer.StartNs + outer.DurationNs);
        CHECK(zone.StartNs >= frame.StartNs);
        CHECK(zone.StartNs + zone.DurationNs <= frame.EndNs);
    }
}

TEST_CASE("Profiler - each thread's zones are kept apart under its own name")
{
    Recording recording;
    SetCurrentThreadName("Main");
    {
        HH_PROFILE_ZONE("MainWork");
        std::thread worker([] {
            SetCurrentThreadName("Worker");
            HH_PROFILE_ZONE("WorkerWork");
            HH_PROFILE_ZONE("WorkerStep");
        });
        worker.join();
    }
    MarkFrame();

    const FrameRecord&  frame  = GetFrame(0);
    const ThreadRecord* main   = FindThread(frame, "Main");
    const ThreadRecord* worker = FindThread(frame, "Worker");
    REQUIRE(main != nullptr);
    REQUIRE(worker != nullptr);
    CHECK(main->ThreadIndex != worker->ThreadIndex);
    REQUIRE(main->Zones.size() == 1);
    CHECK(Named(main->Zones[0], "MainWork"));
    REQUIRE(worker->Zones.size() == 2);
    CHECK(Named(worker->Zones[0], "WorkerStep"));
    CHECK(worker->Zones[0].Depth == 1);
    CHECK(Named(worker->Zones[1], "WorkerWork"));
    CHECK(worker->Zones[1].Depth == 0);
}

TEST_CASE("Profiler - the ring keeps the newest frames, oldest dropped")
{
    Recording recording;
    SetFrameHistory(3);
    SetCurrentThreadName("Main");

    const char* names[] = { "F0", "F1", "F2", "F3", "F4" };
    for (const char* name : names)
    {
        {
            HH_PROFILE_ZONE(name);
        }
        MarkFrame();
    }

    REQUIRE(GetFrameCount() == 3);
    const char* expected[] = { "F4", "F3", "F2" };
    for (size_t age = 0; age < 3; ++age)
    {
        const FrameRecord& frame = GetFrame(age);
        const ThreadRecord* main = FindThread(frame, "Main");
        REQUIRE(main != nullptr);
        REQUIRE(main->Zones.size() == 1);
        CHECK(Named(main->Zones[0], expected[age]));
        if (age > 0)
        {
            CHECK(GetFrame(age - 1).Index == frame.Index + 1);
            CHECK(GetFrame(age - 1).StartNs == frame.EndNs);
        }
    }
    SetFrameHistory(DEFAULT_FRAME_HISTORY);
}

TEST_CASE("Profiler - a disabled recorder records nothing, and a frame without zones has no threads")
{
    Recording recording;
    SetEnabled(false);
    CHECK_FALSE(IsEnabled());
    CHECK_FALSE(BeginZone("Ignored"));
    {
        HH_PROFILE_ZONE("Ignored");
    }
    MarkFrame();
    CHECK(GetFrameCount() == 0);

    SetEnabled(true);
    MarkFrame();
    REQUIRE(GetFrameCount() == 1);
    CHECK(GetFrame(0).Threads.empty());
}

TEST_CASE("Profiler - a zone open across a frame mark lands in the frame it ends in")
{
    Recording recording;
    SetCurrentThreadName("Main");
    {
        HH_PROFILE_ZONE("Long");
        std::this_thread::sleep_for(std::chrono::milliseconds(1)); // past the clock's tick
        MarkFrame();
    }
    MarkFrame();

    REQUIRE(GetFrameCount() == 2);
    CHECK(FindThread(GetFrame(1), "Main") == nullptr);
    const ThreadRecord* main = FindThread(GetFrame(0), "Main");
    REQUIRE(main != nullptr);
    REQUIRE(main->Zones.size() == 1);
    CHECK(main->Zones[0].StartNs < GetFrame(0).StartNs);
}

TEST_CASE("Profiler - a zone left open while disabled still ends, and Clear drops what is open")
{
    Recording recording;
    SetCurrentThreadName("Main");
    {
        HH_PROFILE_ZONE("BeganEnabled");
        SetEnabled(false);
    }
    SetEnabled(true);
    MarkFrame();
    const ThreadRecord* main = FindThread(GetFrame(0), "Main");
    REQUIRE(main != nullptr);
    CHECK(main->Zones.size() == 1);

    {
        HH_PROFILE_ZONE("Dropped");
        Clear();
    }
    MarkFrame();
    REQUIRE(GetFrameCount() == 1);
    CHECK(GetFrame(0).Threads.empty());
}

TEST_CASE("Profiler - an interned name is one pointer per text and outlives its source")
{
    std::string source = "Pass: Forward";
    const char* name   = InternName(source);
    source             = "something else";
    CHECK(std::string(name) == "Pass: Forward");
    CHECK(InternName("Pass: Forward") == name);
    CHECK(InternName(std::string_view("Pass: Forward!").substr(0, 13)) == name);
    CHECK(InternName("Pass: Shadow") != name);
}
