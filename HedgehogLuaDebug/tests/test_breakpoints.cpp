#include "doctest/doctest/doctest.h"

#include "HedgehogLuaDebug/api/DebugServer.hpp"
#include "HedgehogLuaDebug/api/LuaDebugEngine.hpp"

#include "FileSystem/tests/test_helpers.hpp"

#include "tinygltf/json.hpp"

extern "C"
{
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using nlohmann::json;

namespace
{
    // Line numbers matter: the tests set breakpoints by them.
    constexpr const char* SCRIPT = "-- A test script.\n"      // 1
                                   "\n"                       // 2
                                   "local function add(a, b)\n" // 3
                                   "    local c = a + b\n"    // 4
                                   "    return c\n"           // 5
                                   "end\n"                    // 6
                                   "\n"                       // 7
                                   "-- comment\n"             // 8
                                   "result = add(1, 2)\n";    // 9

    // A VM with the script under a temp assets://, a server over an in-memory transport and an
    // engine attached to the VM: the client's requests are queued before the script runs and
    // answered while it is stopped.
    struct DebugSession
    {
        TempDir                      Assets;
        std::string                  ScriptPath;
        LuaDebug::InMemoryTransport* Client = nullptr;
        LuaDebug::DebugServer        Server;
        LuaDebug::LuaDebugEngine     Engine;
        lua_State*                   State = luaL_newstate();
        int                          Seq   = 0;

        DebugSession()
            : Server(MakeTransport(Client))
            , Engine(Server, LuaDebug::SourceMapper([this](const std::string& virtualPath) -> std::optional<std::filesystem::path>
                                                    {
                                                        const std::string prefix = "assets://";
                                                        if (!virtualPath.starts_with(prefix))
                                                            return std::nullopt;
                                                        return Assets.Path() / virtualPath.substr(prefix.size());
                                                    }))
        {
            ScriptPath = Assets.WriteFile("Scripts/Test.lua", SCRIPT).string();
            luaL_openlibs(State);
            REQUIRE(Server.Start());
            Engine.Attach(State);
        }

        ~DebugSession()
        {
            Engine.Detach();
            lua_close(State);
        }

        static std::unique_ptr<LuaDebug::ITransport> MakeTransport(LuaDebug::InMemoryTransport*& client)
        {
            auto transport = std::make_unique<LuaDebug::InMemoryTransport>();
            client         = transport.get();
            return transport;
        }

        void Queue(const std::string& command, json arguments = json::object())
        {
            Client->PushFromClient(json{ { "seq", ++Seq }, { "type", "request" }, { "command", command }, { "arguments", arguments } }.dump());
        }

        std::vector<json> Sent()
        {
            std::vector<json> messages;
            for (const std::string& body : Client->TakeSent())
                messages.push_back(json::parse(body));
            return messages;
        }

        json SetBreakpoints(const std::vector<int>& lines)
        {
            json breakpoints = json::array();
            for (const int line : lines)
                breakpoints.push_back({ { "line", line } });
            Queue("setBreakpoints", { { "source", { { "path", ScriptPath } } }, { "breakpoints", breakpoints } });
            Server.Pump();
            const std::vector<json> sent = Sent();
            REQUIRE(sent.size() == 1);
            return sent[0];
        }

        // Runs the script as the script system loads files: chunk named by its virtual path.
        void Run()
        {
            REQUIRE(luaL_loadbufferx(State, SCRIPT, std::strlen(SCRIPT), "@assets://Scripts/Test.lua", "t") == LUA_OK);
            REQUIRE(lua_pcall(State, 0, 0, 0) == LUA_OK);
            lua_getglobal(State, "result");
            CHECK(lua_tointeger(State, -1) == 3);
            lua_pop(State, 1);
        }
    };
}

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

    CHECK(sent[2]["body"]["scopes"].size() == 2);
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
