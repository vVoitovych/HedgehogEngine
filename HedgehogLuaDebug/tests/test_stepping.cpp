#include "doctest/doctest/doctest.h"

#include "test_debug_session.hpp"

using LuaDebugTest::DebugSession;
using nlohmann::json;

namespace
{
    // Queues a step, then a stackTrace and continue for the stop it leads to; returns that stop's
    // reason and top frame.
    std::pair<std::string, json> StepOnce(DebugSession& session, const std::string& step)
    {
        session.Queue(step, { { "threadId", 1 } });
        session.Queue("stackTrace", { { "threadId", 1 } });
        session.Queue("continue", { { "threadId", 1 } });
        session.Run();

        std::vector<json> stops;
        json              trace;
        for (const json& message : session.Sent())
        {
            if (message.value("event", "") == "stopped")
                stops.push_back(message);
            if (message.value("command", "") == "stackTrace")
                trace = message;
        }
        REQUIRE(stops.size() == 2); // the breakpoint, then the step
        CHECK(stops[0]["body"]["reason"] == "breakpoint");
        REQUIRE(trace["success"] == true);
        return { stops[1]["body"]["reason"].get<std::string>(), trace["body"]["stackFrames"][0] };
    }

    // Lines matter.
    constexpr const char* COROUTINE_SCRIPT = "co = coroutine.create(function()\n" // 1
                                             "    local x = 1\n"                  // 2
                                             "    coroutine.yield(x)\n"           // 3
                                             "    x = x + 1\n"                    // 4
                                             "end)\n"                             // 5
                                             "local ok, v = coroutine.resume(co)\n" // 6
                                             "done = v\n";                        // 7
}

TEST_CASE("Lua stepping - step over skips the call, step in enters it, step out returns to the caller")
{
    {
        DebugSession session;
        (void)session.SetBreakpoints({ 9 });
        const auto [reason, frame] = StepOnce(session, "next");
        CHECK(reason == "step");
        CHECK(frame["name"] == "main chunk");
        CHECK(frame["line"] == 10);
        CHECK(session.Global("final") == 4);
    }
    {
        DebugSession session;
        (void)session.SetBreakpoints({ 9 });
        const auto [reason, frame] = StepOnce(session, "stepIn");
        CHECK(frame["name"] == "add");
        CHECK(frame["line"] == 4);
    }
    {
        DebugSession session;
        (void)session.SetBreakpoints({ 4 });
        const auto [reason, frame] = StepOnce(session, "stepOut");
        CHECK(frame["name"] == "main chunk");
        CHECK(frame["line"] == 10);
    }
    {
        // Step over inside the function goes to its next line.
        DebugSession session;
        (void)session.SetBreakpoints({ 4 });
        const auto [reason, frame] = StepOnce(session, "next");
        CHECK(frame["name"] == "add");
        CHECK(frame["line"] == 5);
    }
}

TEST_CASE("Lua stepping - a coroutine yield is a frame boundary")
{
    {
        // Stepping over the yield stops in the resumer, at its next line.
        DebugSession session(COROUTINE_SCRIPT, "Co.lua");
        (void)session.SetBreakpoints({ 3 });
        const auto [reason, frame] = StepOnce(session, "next");
        CHECK(frame["name"] == "main chunk");
        CHECK(frame["line"] == 7);
        CHECK(session.Global("done") == 1);
    }
    {
        // Stepping over the resume does not stop inside the coroutine.
        DebugSession session(COROUTINE_SCRIPT, "Co.lua");
        (void)session.SetBreakpoints({ 6 });
        const auto [reason, frame] = StepOnce(session, "next");
        CHECK(frame["name"] == "main chunk");
        CHECK(frame["line"] == 7);
    }
    {
        // Stepping in from the resume line enters the coroutine.
        DebugSession session(COROUTINE_SCRIPT, "Co.lua");
        (void)session.SetBreakpoints({ 6 });
        const auto [reason, frame] = StepOnce(session, "stepIn");
        CHECK(frame["line"] == 2);
    }
}

TEST_CASE("Lua stepping - a step outside a stop fails; the hook goes once the step is done")
{
    DebugSession session;
    session.Queue("next", { { "threadId", 1 } });
    session.Server.Pump();
    const std::vector<json> sent = session.Sent();
    REQUIRE(sent.size() == 1);
    CHECK(sent[0]["message"] == "the script is not stopped");

    (void)session.SetBreakpoints({ 9 });
    (void)session.SetBreakpoints({}); // cleared before running: only the step would keep the hook
    CHECK(lua_gethook(session.State) == nullptr);
}
