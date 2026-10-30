#include "Tools/Profiler/ChromeTrace.hpp"

#include "doctest/doctest/doctest.h"

#include <string>
#include <vector>

using namespace Editor;

TEST_CASE("Chrome trace - zones per thread track and GPU passes in order, in microseconds from the first frame")
{
    constexpr uint64_t MS = 1000000;
    Profiling::FrameRecord frame;
    frame.StartNs = 5 * MS;
    frame.EndNs   = 15 * MS;

    Profiling::ThreadRecord main;
    main.ThreadIndex = 0;
    main.ThreadName  = "Main";
    main.Zones       = { { "Update", 6 * MS, 2 * MS + 500, 1 }, { "Frame", 5 * MS, 10 * MS, 0 } };
    Profiling::ThreadRecord worker;
    worker.ThreadIndex = 3;
    worker.ThreadName  = "Worker \"A\"";
    worker.Zones       = { { "Job", 7 * MS, MS, 0 } };
    frame.Threads      = { main, worker };

    const std::vector<Renderer::GpuPassTiming> passes   = { { "Shadow", 0.25 }, { "Forward", 0.5 } };
    const Profiling::FrameRecord*              frames[] = { &frame };
    const TraceGpuFrame                        gpu[]    = { { 5 * MS, passes } };

    const std::string expected =
        "{\"displayTimeUnit\":\"ms\",\"traceEvents\":["
        "{\"name\":\"process_name\",\"ph\":\"M\",\"pid\":1,\"tid\":0,\"args\":{\"name\":\"CPU\"}},\n"
        "{\"name\":\"thread_name\",\"ph\":\"M\",\"pid\":1,\"tid\":0,\"args\":{\"name\":\"Main\"}},\n"
        "{\"name\":\"thread_name\",\"ph\":\"M\",\"pid\":1,\"tid\":3,\"args\":{\"name\":\"Worker \\\"A\\\"\"}},\n"
        "{\"name\":\"Update\",\"ph\":\"X\",\"pid\":1,\"tid\":0,\"ts\":1000.000,\"dur\":2000.500},\n"
        "{\"name\":\"Frame\",\"ph\":\"X\",\"pid\":1,\"tid\":0,\"ts\":0.000,\"dur\":10000.000},\n"
        "{\"name\":\"Job\",\"ph\":\"X\",\"pid\":1,\"tid\":3,\"ts\":2000.000,\"dur\":1000.000},\n"
        "{\"name\":\"process_name\",\"ph\":\"M\",\"pid\":2,\"tid\":0,\"args\":{\"name\":\"GPU\"}},\n"
        "{\"name\":\"thread_name\",\"ph\":\"M\",\"pid\":2,\"tid\":0,\"args\":{\"name\":\"Render graph passes\"}},\n"
        "{\"name\":\"Shadow\",\"ph\":\"X\",\"pid\":2,\"tid\":0,\"ts\":0.000,\"dur\":250.000},\n"
        "{\"name\":\"Forward\",\"ph\":\"X\",\"pid\":2,\"tid\":0,\"ts\":250.000,\"dur\":500.000}"
        "]}\n";
    CHECK(WriteChromeTrace(frames, gpu) == expected);
}

TEST_CASE("Chrome trace - two frames name each thread once, and nothing captured is an empty trace")
{
    Profiling::FrameRecord first;
    first.Threads = { { 0, "Main", { { "Frame", 0, 100, 0 } } } };
    Profiling::FrameRecord second      = first;
    second.StartNs                     = 100;
    second.Threads[0].Zones[0].StartNs = 100;
    const Profiling::FrameRecord* frames[] = { &first, &second };

    const std::string trace = WriteChromeTrace(frames, {});
    CHECK(trace.find("\"thread_name\"") == trace.rfind("\"thread_name\""));
    CHECK(trace.find("\"ts\":0.100") != std::string::npos);
    CHECK(trace.find("GPU") == std::string::npos);

    CHECK(WriteChromeTrace({}, {}) ==
          "{\"displayTimeUnit\":\"ms\",\"traceEvents\":["
          "{\"name\":\"process_name\",\"ph\":\"M\",\"pid\":1,\"tid\":0,\"args\":{\"name\":\"CPU\"}}]}\n");
}

TEST_CASE("Chrome trace - a frame's GPU commands go one after another on their own track, their pass in args")
{
    constexpr uint64_t MS = 1000000;
    Profiling::FrameRecord frame;
    frame.StartNs = 2 * MS;
    frame.EndNs   = 4 * MS;

    const std::vector<Renderer::GpuPassTiming>    passes   = { { "Forward", 0.5 } };
    const std::vector<Renderer::GpuCommandTiming> commands = {
        { "Forward", Renderer::GpuCommandKind::Barrier, 1, 1, 0.125 },
        { "Forward", Renderer::GpuCommandKind::DrawIndexed, 36, 2, 0.25 },
    };
    const Profiling::FrameRecord* frames[] = { &frame };
    const TraceGpuFrame           gpu[]    = { { 2 * MS, passes, commands } };

    const std::string expected =
        "{\"displayTimeUnit\":\"ms\",\"traceEvents\":["
        "{\"name\":\"process_name\",\"ph\":\"M\",\"pid\":1,\"tid\":0,\"args\":{\"name\":\"CPU\"}},\n"
        "{\"name\":\"process_name\",\"ph\":\"M\",\"pid\":2,\"tid\":0,\"args\":{\"name\":\"GPU\"}},\n"
        "{\"name\":\"thread_name\",\"ph\":\"M\",\"pid\":2,\"tid\":0,\"args\":{\"name\":\"Render graph passes\"}},\n"
        "{\"name\":\"thread_name\",\"ph\":\"M\",\"pid\":2,\"tid\":1,\"args\":{\"name\":\"GPU commands\"}},\n"
        "{\"name\":\"Forward\",\"ph\":\"X\",\"pid\":2,\"tid\":0,\"ts\":0.000,\"dur\":500.000},\n"
        "{\"name\":\"Barrier 1 transition\",\"ph\":\"X\",\"pid\":2,\"tid\":1,\"ts\":0.000,\"dur\":125.000,"
        "\"args\":{\"pass\":\"Forward\"}},\n"
        "{\"name\":\"DrawIndexed 36 indices x 2 instances\",\"ph\":\"X\",\"pid\":2,\"tid\":1,\"ts\":125.000,"
        "\"dur\":250.000,\"args\":{\"pass\":\"Forward\"}}"
        "]}\n";
    CHECK(WriteChromeTrace(frames, gpu) == expected);
}
