#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace FS
{
    class FileSystemManager;
}

namespace HedgehogEngine
{
    class EngineContext;
}

namespace HedgehogSettings
{
    struct LuaDebuggerSettings;
}

namespace LuaDebug
{
    class DebugServer;
    class LuaDebugEngine;
}

namespace HedgehogScripting
{
    class ScriptSystem;

    // VS Code's way into the script system's VM (epic HE-180): a HedgehogLuaDebug server on
    // 127.0.0.1:<port> and an engine attached to the ScriptSystem's one Lua state, its chunk names
    // (@assets://...) resolved through the script file system. Pump it once a frame on the game
    // thread. It mirrors every Logger line to the client's Debug Console (output events), and
    // reports Play as the Lua thread starting and Stop as it exiting. Created only when
    // engine_settings.yaml enables it (StartScriptDebugger), so a disabled debugger costs nothing.
    class ScriptDebugger
    {
    public:
        ScriptDebugger(HedgehogEngine::EngineContext& context, ScriptSystem& scripts, const FS::FileSystemManager& scriptFiles,
                       uint16_t port);
        ~ScriptDebugger();

        ScriptDebugger(const ScriptDebugger&)            = delete;
        ScriptDebugger& operator=(const ScriptDebugger&) = delete;

        // Listens; false, logged, when the port cannot be opened.
        [[nodiscard]] bool Start();
        // Handles the client's requests, sends the Logger's output and the thread events.
        void Pump();

        // See LuaDebug::LuaDebugEngine::SetStopCallbacks; the output keeps flowing while stopped.
        void SetStopCallbacks(std::function<void(bool stopped)> stopChanged, std::function<void()> whileStopped);
        [[nodiscard]] bool IsStopped() const;

    private:
        void FlushOutput();

    private:
        HedgehogEngine::EngineContext&            m_Context;
        std::unique_ptr<LuaDebug::DebugServer>    m_Server;
        std::unique_ptr<LuaDebug::LuaDebugEngine> m_Engine;
        int                                       m_LogSink    = 0;
        bool                                      m_WasPlaying = false;

        // Logger lines from any thread, sent from the game thread.
        std::mutex               m_OutputMutex;
        std::vector<std::string> m_Output;
    };

    // The debugger when the settings enable it, started; nullptr when they do not or it cannot
    // listen (logged), in which case nothing is created.
    [[nodiscard]] std::unique_ptr<ScriptDebugger> StartScriptDebugger(HedgehogEngine::EngineContext&           context,
                                                                      ScriptSystem&                            scripts,
                                                                      const FS::FileSystemManager&             scriptFiles,
                                                                      const HedgehogSettings::LuaDebuggerSettings& settings);
}
