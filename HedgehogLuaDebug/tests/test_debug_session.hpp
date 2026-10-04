#pragma once

#include "HedgehogLuaDebug/api/DebugServer.hpp"
#include "HedgehogLuaDebug/api/LuaDebugEngine.hpp"

#include "FileSystem/tests/test_helpers.hpp"

#include "doctest/doctest/doctest.h"

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

namespace LuaDebugTest
{
    using nlohmann::json;

    // Line numbers matter: the tests set breakpoints by them.
    constexpr const char* SCRIPT = "-- A test script.\n"      // 1
                                   "\n"                       // 2
                                   "local function add(a, b)\n" // 3
                                   "    local c = a + b\n"    // 4
                                   "    return c\n"           // 5
                                   "end\n"                    // 6
                                   "\n"                       // 7
                                   "-- comment\n"             // 8
                                   "result = add(1, 2)\n"     // 9
                                   "final = result + 1\n";    // 10

    // A VM with a script under a temp assets://, a server over an in-memory transport and an
    // engine attached to the VM: the client's requests are queued before the script runs and
    // answered while it is stopped.
    struct DebugSession
    {
        TempDir                      Assets;
        std::string                  ScriptPath;
        std::string                  Script;
        std::string                  ChunkName;
        LuaDebug::InMemoryTransport* Client = nullptr;
        LuaDebug::DebugServer        Server;
        LuaDebug::LuaDebugEngine     Engine;
        lua_State*                   State = luaL_newstate();
        int                          Seq   = 0;

        explicit DebugSession(const std::string& script = SCRIPT, const std::string& fileName = "Test.lua")
            : Script(script)
            , ChunkName("@assets://Scripts/" + fileName)
            , Server(MakeTransport(Client))
            , Engine(Server, LuaDebug::SourceMapper([this](const std::string& virtualPath) -> std::optional<std::filesystem::path>
                                                    {
                                                        const std::string prefix = "assets://";
                                                        if (!virtualPath.starts_with(prefix))
                                                            return std::nullopt;
                                                        return Assets.Path() / virtualPath.substr(prefix.size());
                                                    }))
        {
            ScriptPath = Assets.WriteFile("Scripts/" + fileName, script).string();
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
            REQUIRE(luaL_loadbufferx(State, Script.data(), Script.size(), ChunkName.c_str(), "t") == LUA_OK);
            REQUIRE(lua_pcall(State, 0, 0, 0) == LUA_OK);
        }

        lua_Integer Global(const char* name)
        {
            lua_getglobal(State, name);
            const lua_Integer value = lua_tointeger(State, -1);
            lua_pop(State, 1);
            return value;
        }
    };
}

