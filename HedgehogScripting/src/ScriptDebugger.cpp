#include "HedgehogScripting/api/ScriptDebugger.hpp"
#include "HedgehogScripting/api/ScriptSystem.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/HedgehogSettings/api/LuaDebuggerSettings.hpp"

#include "HedgehogLuaDebug/api/DebugServer.hpp"
#include "HedgehogLuaDebug/api/LuaDebugEngine.hpp"
#include "HedgehogLuaDebug/api/TcpTransport.hpp"

#include "FileSystem/api/FileSystemManager.hpp"
#include "Logger/api/Logger.hpp"

#include <cstdio>

namespace HedgehogScripting
{
    namespace
    {
        // Lines kept for the client between frames; a flood beyond it drops the oldest.
        constexpr size_t MAX_PENDING_OUTPUT = 1000;

        const char* LevelPrefix(EngineLogger::LogLevel level)
        {
            switch (level)
            {
            case EngineLogger::LogLevel::Verbose: return "[VERBOSE] ";
            case EngineLogger::LogLevel::Warning: return "[WARNING] ";
            case EngineLogger::LogLevel::Error:   return "[ERROR] ";
            case EngineLogger::LogLevel::Info:    break;
            }
            return "[INFO] ";
        }

        // A JSON string literal, for the output event's body.
        std::string Quote(const std::string& text)
        {
            std::string quoted = "\"";
            for (const char c : text)
            {
                switch (c)
                {
                case '"':  quoted += "\\\""; break;
                case '\\': quoted += "\\\\"; break;
                case '\n': quoted += "\\n"; break;
                case '\r': quoted += "\\r"; break;
                case '\t': quoted += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20)
                    {
                        char escaped[8];
                        std::snprintf(escaped, sizeof(escaped), "\\u%04x", c);
                        quoted += escaped;
                    }
                    else
                    {
                        quoted += c;
                    }
                }
            }
            return quoted + "\"";
        }
    }

    ScriptDebugger::ScriptDebugger(HedgehogEngine::EngineContext& context, ScriptSystem& scripts,
                                   const FS::FileSystemManager& scriptFiles, uint16_t port)
        : m_Context(context)
        , m_Server(std::make_unique<LuaDebug::DebugServer>(std::make_unique<LuaDebug::TcpTransport>(port)))
    {
        m_Engine = std::make_unique<LuaDebug::LuaDebugEngine>(
            *m_Server, LuaDebug::SourceMapper([&scriptFiles](const std::string& virtualPath) { return scriptFiles.ResolvePhysical(virtualPath); }));
        m_Engine->Attach(scripts.GetLuaState());
    }

    ScriptDebugger::~ScriptDebugger()
    {
        if (m_LogSink != 0)
            EngineLogger::Logger::Instance().RemoveSink(m_LogSink);
        m_Engine.reset();
        m_Server->Stop();
    }

    bool ScriptDebugger::Start()
    {
        if (!m_Server->Start())
            return false;
        m_LogSink = EngineLogger::Logger::Instance().AddSink(
            [this](EngineLogger::LogLevel level, const std::string& message)
            {
                std::lock_guard lock(m_OutputMutex);
                if (m_Output.size() >= MAX_PENDING_OUTPUT)
                    m_Output.erase(m_Output.begin());
                m_Output.push_back(LevelPrefix(level) + message + "\n");
            });
        return true;
    }

    void ScriptDebugger::FlushOutput()
    {
        std::vector<std::string> lines;
        {
            std::lock_guard lock(m_OutputMutex);
            lines.swap(m_Output);
        }
        if (!m_Server->GetTransport().IsConnected())
            return;
        for (const std::string& line : lines)
            m_Server->SendEvent("output", "{\"category\":\"console\",\"output\":" + Quote(line) + "}");
    }

    void ScriptDebugger::Pump()
    {
        // Play is the Lua thread's life as the client sees it.
        const bool playing = m_Context.GetPlayState() != HedgehogEngine::PlayState::Edit;
        if (playing != m_WasPlaying)
        {
            m_WasPlaying = playing;
            m_Server->SendEvent("thread", std::string("{\"reason\":\"") + (playing ? "started" : "exited") + "\",\"threadId\":1}");
        }
        m_Server->Pump();
        FlushOutput();
    }

    void ScriptDebugger::SetStopCallbacks(std::function<void(bool stopped)> stopChanged, std::function<void()> whileStopped)
    {
        m_Engine->SetStopCallbacks(std::move(stopChanged),
                                   [this, whileStopped = std::move(whileStopped)]()
                                   {
                                       FlushOutput();
                                       if (whileStopped)
                                           whileStopped();
                                   });
    }

    bool ScriptDebugger::IsStopped() const { return m_Engine->IsStopped(); }

    std::unique_ptr<ScriptDebugger> StartScriptDebugger(HedgehogEngine::EngineContext& context, ScriptSystem& scripts,
                                                        const FS::FileSystemManager&                 scriptFiles,
                                                        const HedgehogSettings::LuaDebuggerSettings& settings)
    {
        if (!settings.Enabled)
            return nullptr;
        auto debugger = std::make_unique<ScriptDebugger>(context, scripts, scriptFiles, settings.Port);
        if (!debugger->Start())
            return nullptr;
        debugger->SetStopCallbacks({}, {});
        return debugger;
    }
}
