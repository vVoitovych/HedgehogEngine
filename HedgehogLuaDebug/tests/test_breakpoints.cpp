#include "doctest/doctest/doctest.h"

#include "test_debug_session.hpp"

using LuaDebugTest::DebugSession;
using LuaDebugTest::SCRIPT;
using nlohmann::json;

TEST_CASE("Lua debugger - valid lines cover nested functions; breakpoints move to the next one")
{
    const std::vector<int> lines = LuaDebug::LuaDebugEngine::FindValidLines(SCRIPT);
    CHECK(std::find(lines.begin(), lines.end(), 4) != lines.end()); // inside add
    CHECK(std::find(lines.begin(), lines.end(), 9) != lines.end());
    CHECK(std::find(lines.begin(), lines.end(), 1) == lines.end()); // a comment
    CHECK(std::find(lines.begin(), lines.end(), 8) == lines.end());
    CHECK(LuaDebug::LuaDebugEngine::FindValidLines("this is not lua").empty());

    DebugSession session;
    const json   response = session.SetBreakpoints({ 4, 8, 100 });
    CHECK(response["success"] == true);
    const json& placed = response["body"]["breakpoints"];
    REQUIRE(placed.size() == 3);
    CHECK(placed[0] == json{ { "verified", true }, { "line", 4 } });
    CHECK(placed[1] == json{ { "verified", true }, { "line", 9 } }); // the comment's breakpoint moves down
    CHECK(placed[2]["verified"] == false);
    CHECK(placed[2]["line"] == 100);
    CHECK(placed[2]["message"] == "no code at or after this line");
}

TEST_CASE("Lua debugger - with no breakpoints no hook is installed")
{
    DebugSession session;
    CHECK(lua_gethook(session.State) == nullptr);
    CHECK_FALSE(session.Engine.IsHookInstalled());

    (void)session.SetBreakpoints({ 4 });
    CHECK(lua_gethook(session.State) != nullptr);

    (void)session.SetBreakpoints({});
    CHECK(lua_gethook(session.State) == nullptr);
    session.Run(); // runs through without stopping
    CHECK(session.Global("result") == 3);
    CHECK(session.Sent().empty());
}

TEST_CASE("Lua debugger - a breakpoint in a function stops there and stackTrace reports the frames")
{
    DebugSession session;
    (void)session.SetBreakpoints({ 4 });

    session.Queue("stackTrace", { { "threadId", 1 } });
    session.Queue("scopes", { { "frameId", 1 } });
    session.Queue("continue", { { "threadId", 1 } });
    session.Run();
    CHECK_FALSE(session.Engine.IsStopped());

    const std::vector<json> sent = session.Sent();
    REQUIRE(sent.size() == 4);
    CHECK(sent[0]["type"] == "event");
    CHECK(sent[0]["event"] == "stopped");
    CHECK(sent[0]["body"]["reason"] == "breakpoint");
    CHECK(sent[0]["body"]["threadId"] == 1);

    const json& trace = sent[1];
    CHECK(trace["command"] == "stackTrace");
    CHECK(trace["success"] == true);
    const json& frames = trace["body"]["stackFrames"];
    REQUIRE(frames.size() == 2);
    const std::string path = LuaDebug::SourceMapper::NormalizePath(session.ScriptPath);
    CHECK(frames[0]["name"] == "add");
    CHECK(frames[0]["line"] == 4);
    CHECK(frames[0]["source"]["path"] == path);
    CHECK(frames[0]["source"]["name"] == "test.lua");
    CHECK(frames[1]["name"] == "main chunk");
    CHECK(frames[1]["line"] == 9);
    CHECK(frames[1]["source"]["path"] == path);
    CHECK(trace["body"]["totalFrames"] == 2);

    CHECK(sent[2]["body"]["scopes"].size() == 3);
    CHECK(sent[3]["command"] == "continue");
    CHECK(sent[3]["success"] == true);

    // Outside a stop, stackTrace fails.
    session.Queue("stackTrace", { { "threadId", 1 } });
    session.Server.Pump();
    CHECK(session.Sent()[0]["message"] == "the script is not stopped");
}

TEST_CASE("Lua debugger - pause stops at the next line, then the hook goes away")
{
    DebugSession session;
    session.Queue("pause", { { "threadId", 1 } });
    session.Server.Pump();
    CHECK(session.Engine.IsHookInstalled());
    (void)session.Sent();

    session.Queue("continue");
    session.Run();
    const std::vector<json> sent = session.Sent();
    REQUIRE(sent.size() == 2);
    CHECK(sent[0]["body"]["reason"] == "pause");
    CHECK(lua_gethook(session.State) == nullptr);
}

TEST_CASE("Lua debugger - a client that disconnects while stopped resumes the script and clears its breakpoints")
{
    DebugSession session;
    (void)session.SetBreakpoints({ 5 });
    session.Queue("disconnect");
    session.Run();
    CHECK_FALSE(session.Engine.IsStopped());
    CHECK_FALSE(session.Client->IsConnected());
    CHECK(lua_gethook(session.State) == nullptr);
}
