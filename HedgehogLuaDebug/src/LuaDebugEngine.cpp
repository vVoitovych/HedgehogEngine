#include "HedgehogLuaDebug/api/LuaDebugEngine.hpp"
#include "HedgehogLuaDebug/api/DebugServer.hpp"

#include "tinygltf/json.hpp"

// Lua is compiled as C. Its own headers (ldebug, lobject, lstate) give the line table of every
// function in a chunk, which the public API reaches only for the outermost one; Lua is built
// from source into this engine, so they match it.
extern "C"
{
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include "ldebug.h"
#include "lobject.h"
#include "lstate.h"
}

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <thread>

namespace LuaDebug
{
    namespace
    {
        // The registry key under which a VM keeps its engine, for the hook.
        const char ENGINE_KEY = 0;

        constexpr auto STOPPED_POLL_INTERVAL = std::chrono::milliseconds(2);

        void CollectLines(const Proto* proto, std::set<int>& lines)
        {
            // A vararg function (every main chunk) starts with VARARGPREP, which carries the
            // function's first line but never triggers a line hook.
            for (int pc = proto->is_vararg ? 1 : 0; pc < proto->sizecode; ++pc)
                lines.insert(luaG_getfuncline(proto, pc));
            for (int i = 0; i < proto->sizep; ++i)
                CollectLines(proto->p[i], lines);
        }
    }

    LuaDebugEngine::LuaDebugEngine(DebugServer& server, SourceMapper mapper)
        : m_Server(server)
        , m_Mapper(std::move(mapper))
    {
        m_Server.SetEngine(this);
    }

    LuaDebugEngine::~LuaDebugEngine()
    {
        Detach();
        m_Server.SetEngine(nullptr);
    }

    void LuaDebugEngine::Attach(lua_State* state)
    {
        Detach();
        m_State = state;
        lua_pushlightuserdata(m_State, this);
        lua_rawsetp(m_State, LUA_REGISTRYINDEX, &ENGINE_KEY);
        UpdateHook();
    }

    void LuaDebugEngine::Detach()
    {
        if (!m_State)
            return;
        lua_sethook(m_State, nullptr, 0, 0);
        lua_pushnil(m_State);
        lua_rawsetp(m_State, LUA_REGISTRYINDEX, &ENGINE_KEY);
        m_State         = nullptr;
        m_HookInstalled = false;
    }

    bool LuaDebugEngine::IsHookInstalled() const { return m_HookInstalled; }

    bool LuaDebugEngine::IsStopped() const { return m_Stopped; }

    void LuaDebugEngine::UpdateHook()
    {
        const bool wanted = m_State && (m_PauseRequested || !m_Breakpoints.empty());
        if (wanted == m_HookInstalled)
            return;
        if (m_State)
            lua_sethook(m_State, wanted ? &LuaDebugEngine::Hook : nullptr, wanted ? LUA_MASKLINE : 0, 0);
        m_HookInstalled = wanted;
    }

    std::vector<int> LuaDebugEngine::FindValidLines(const std::string& source)
    {
        lua_State*       state = luaL_newstate();
        std::vector<int> result;
        if (luaL_loadbufferx(state, source.data(), source.size(), "=source", "t") == LUA_OK)
        {
            std::set<int> lines;
            CollectLines(clLvalue(s2v(state->top.p - 1))->p, lines);
            result.assign(lines.begin(), lines.end());
        }
        lua_close(state);
        return result;
    }

    std::vector<BreakpointResult> LuaDebugEngine::SetBreakpoints(const std::string& path, const std::vector<int>& lines)
    {
        const std::string key = SourceMapper::NormalizePath(path);
        std::string       source;
        if (std::ifstream file{ std::filesystem::path(path), std::ios::binary })
            source.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        const std::vector<int> valid = FindValidLines(source);

        std::vector<BreakpointResult> results;
        std::set<int>                 placed;
        for (const int line : lines)
        {
            const auto next = std::lower_bound(valid.begin(), valid.end(), line);
            if (next == valid.end())
            {
                results.push_back({ line, false, valid.empty() ? "the file cannot be read or does not compile"
                                                               : "no code at or after this line" });
                continue;
            }
            placed.insert(*next);
            results.push_back({ *next, true, {} });
        }
        if (placed.empty())
            m_Breakpoints.erase(key);
        else
            m_Breakpoints[key] = std::move(placed);
        UpdateHook();
        return results;
    }

    void LuaDebugEngine::RequestPause()
    {
        m_PauseRequested = true;
        UpdateHook();
    }

    void LuaDebugEngine::Continue() { m_Stopped = false; }

    void LuaDebugEngine::ClearSession()
    {
        m_Breakpoints.clear();
        m_PauseRequested = false;
        m_Stopped        = false;
        UpdateHook();
    }

    void LuaDebugEngine::Hook(lua_State* state, lua_Debug* debug)
    {
        lua_rawgetp(state, LUA_REGISTRYINDEX, &ENGINE_KEY);
        auto* engine = static_cast<LuaDebugEngine*>(lua_touserdata(state, -1));
        lua_pop(state, 1);
        if (engine && debug->event == LUA_HOOKLINE)
            engine->OnLine(state, debug);
    }

    void LuaDebugEngine::OnLine(lua_State* state, lua_Debug* debug)
    {
        if (m_Stopped)
            return;
        if (m_PauseRequested)
        {
            m_PauseRequested = false;
            UpdateHook();
            StopAndWait(state, "pause");
            return;
        }
        if (!lua_getinfo(state, "S", debug))
            return;
        const auto file = m_Breakpoints.find(m_Mapper.ToPath(debug->source));
        if (file != m_Breakpoints.end() && file->second.contains(debug->currentline))
            StopAndWait(state, "breakpoint");
    }

    void LuaDebugEngine::StopAndWait(lua_State* state, const char* reason)
    {
        m_Stopped      = true;
        m_StoppedState = state;
        m_Server.SendEvent("stopped", nlohmann::json{ { "reason", reason }, { "threadId", 1 }, { "allThreadsStopped", true } }.dump());

        // The game thread waits here, answering the client, until it continues or leaves.
        while (m_Stopped)
        {
            m_Server.Pump();
            if (!m_Server.GetTransport().IsConnected())
            {
                ClearSession();
                break;
            }
            if (m_Stopped)
                std::this_thread::sleep_for(STOPPED_POLL_INTERVAL);
        }
        m_StoppedState = nullptr;
    }

    std::vector<StackFrameInfo> LuaDebugEngine::GetStackTrace()
    {
        std::vector<StackFrameInfo> frames;
        if (!m_StoppedState)
            return frames;
        lua_Debug debug{};
        for (int level = 0; lua_getstack(m_StoppedState, level, &debug); ++level)
        {
            lua_getinfo(m_StoppedState, "Sln", &debug);
            StackFrameInfo frame;
            frame.Id   = level + 1;
            frame.Line = debug.currentline;
            frame.Path = m_Mapper.ToPath(debug.source);
            if (debug.name)
                frame.Name = debug.name;
            else if (debug.what && std::string_view(debug.what) == "main")
                frame.Name = "main chunk";
            else
                frame.Name = "?";
            frames.push_back(std::move(frame));
        }
        return frames;
    }
}
