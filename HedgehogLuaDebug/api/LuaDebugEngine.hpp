#pragma once

#include "HedgehogLuaDebug/api/SourceMapper.hpp"

#include <map>
#include <optional>
#include <memory>
#include <set>
#include <string>
#include <vector>

struct lua_State;
struct lua_Debug;

namespace LuaDebug
{
    class DebugServer;
    class VariableStore;

    struct BreakpointResult
    {
        int         Line     = 0; // where it was placed: the requested line or the next valid one
        bool        Verified = false;
        std::string Message;      // why it is not verified
    };

    struct StackFrameInfo
    {
        int         Id   = 0; // the frame's level plus one, 0 being invalid in the protocol
        std::string Name;     // the function's name, "main chunk" or "?"
        std::string Path;     // normalized file path, empty for C functions and string chunks
        int         Line = 0;
    };

    // A variable as the client shows it: Reference is nonzero for a table, which expands into its
    // own variables. References are valid only during the stop that made them.
    struct VariableInfo
    {
        std::string Name;
        std::string Value;
        std::string Type;
        int         Reference = 0;
    };

    struct ScopeInfo
    {
        std::string Name;
        int         Reference = 0;
    };

    // An evaluate or setVariable result: the value, or why there is none.
    struct VariableResult
    {
        bool         Ok = false;
        std::string  Error;
        VariableInfo Value;
    };

    enum class StepKind
    {
        None,
        Over, // the next line at this depth or shallower
        In,   // the next line anywhere
        Out,  // the next line shallower than this one
    };

    // Stops one Lua VM on breakpoints (epic HE-180). It registers itself with the DebugServer,
    // whose dispatcher turns setBreakpoints, stackTrace, scopes, continue and pause into calls here.
    //
    // A line hook (lua_sethook) is installed only while a breakpoint is set or a pause is pending,
    // and removed as soon as neither is, so an attached but idle debugger costs nothing. When a
    // line with a breakpoint runs, the engine sends the stopped event and blocks the game thread
    // in a loop that pumps the server until the client continues or leaves; the stack is read
    // there, on the thread that owns the VM. A coroutine created before the hook was installed
    // does not inherit it.
    class LuaDebugEngine
    {
    public:
        LuaDebugEngine(DebugServer& server, SourceMapper mapper);
        ~LuaDebugEngine();

        LuaDebugEngine(const LuaDebugEngine&)            = delete;
        LuaDebugEngine& operator=(const LuaDebugEngine&) = delete;

        // The VM to debug (the script system's one state); Detach removes the hook.
        void Attach(lua_State* state);
        void Detach();

        [[nodiscard]] bool IsHookInstalled() const;
        [[nodiscard]] bool IsStopped() const;

        // Replaces the breakpoints of one file (a path as the client names it); each line moves to
        // the next line that has code, and is unverified when none follows.
        std::vector<BreakpointResult> SetBreakpoints(const std::string& path, const std::vector<int>& lines);
        // Stops at the next line that runs.
        void RequestPause();
        // Leaves the stop and stops again at the step's next line (depths compared within the
        // stopped thread; once that thread yields or ends, the next line in another thread stops,
        // so a coroutine yield is a frame boundary). Only while stopped.
        void Step(StepKind kind);
        // Leaves the stop.
        void Continue();
        // The client is gone: no breakpoints, no pause, and a stop is left.
        void ClearSession();

        // The stopped thread's frames, innermost first; empty when not stopped.
        [[nodiscard]] std::vector<StackFrameInfo> GetStackTrace();

        // Inspection while stopped (frameId as GetStackTrace gives it): a frame's Locals,
        // Upvalues and Globals (its _ENV) scopes; a reference's variables (nullopt for a reference
        // not from this stop), count 0 meaning all; a name or field path ("a.b[3].c", "t['k']")
        // resolved without calling anything Lua-defined (tables are read raw; a userdata's own
        // __index, such as a Vector3's fields, is used); and a number, string or boolean assigned
        // to a local, an upvalue or a table field.
        [[nodiscard]] std::vector<ScopeInfo> GetScopes(int frameId);
        [[nodiscard]] std::optional<std::vector<VariableInfo>> GetVariables(int reference, int start = 0, int count = 0);
        [[nodiscard]] VariableResult Evaluate(const std::string& expression, int frameId);
        [[nodiscard]] VariableResult SetVariable(int reference, const std::string& name, const std::string& value);

        // The lines of a Lua source that hold code (every function in it, nested ones included),
        // sorted; empty when it does not compile.
        [[nodiscard]] static std::vector<int> FindValidLines(const std::string& source);

    private:
        static void Hook(lua_State* state, lua_Debug* debug);
        void OnLine(lua_State* state, lua_Debug* debug);
        void StopAndWait(lua_State* state, const char* reason);
        void UpdateHook();
        [[nodiscard]] bool StepReached(lua_State* state) const;
        void EndStep();

    private:
        DebugServer&  m_Server;
        SourceMapper  m_Mapper;
        lua_State*    m_State        = nullptr;
        lua_State*    m_StoppedState = nullptr;
        bool          m_HookInstalled  = false;
        bool          m_PauseRequested = false;
        bool          m_Stopped        = false;

        StepKind   m_Step          = StepKind::None;
        lua_State* m_StepThread    = nullptr;
        int        m_StepThreadRef = -2; // LUA_NOREF: the thread anchored in the registry while stepping
        int        m_StepDepth     = 0;

        std::unique_ptr<VariableStore> m_Variables;

        std::map<std::string, std::set<int>> m_Breakpoints; // by normalized path
    };
}
