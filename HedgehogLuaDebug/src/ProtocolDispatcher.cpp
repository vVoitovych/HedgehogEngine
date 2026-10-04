#include "ProtocolDispatcher.hpp"

#include "HedgehogLuaDebug/api/LuaDebugEngine.hpp"

#include "Logger/api/Logger.hpp"

#include <filesystem>

namespace LuaDebug
{
    namespace
    {
        // The one thread the debugger reports: scripts run on the game thread.
        constexpr int         LUA_THREAD_ID   = 1;
        constexpr const char* LUA_THREAD_NAME = "Lua";

        // Variables references for a frame's scopes; the variables request comes with the
        // inspection of locals.
        constexpr int SCOPES_PER_FRAME = 2;
    }

    nlohmann::json ProtocolDispatcher::MakeCapabilities()
    {
        // Only what this server does; later tickets turn on breakpoints, stepping and variables.
        return {
            { "supportsConfigurationDoneRequest", true },
            { "supportsFunctionBreakpoints", false },
            { "supportsConditionalBreakpoints", false },
            { "supportsHitConditionalBreakpoints", false },
            { "supportsLogPoints", false },
            { "supportsDataBreakpoints", false },
            { "supportsEvaluateForHovers", false },
            { "supportsSetVariable", false },
            { "supportsStepBack", false },
            { "supportsRestartRequest", false },
            { "supportsTerminateRequest", false },
        };
    }

    nlohmann::json ProtocolDispatcher::MakeResponse(const nlohmann::json& request, bool success)
    {
        return {
            { "seq", ++m_Seq },
            { "type", "response" },
            { "request_seq", request.value("seq", 0) },
            { "success", success },
            { "command", request.value("command", std::string()) },
        };
    }

    nlohmann::json ProtocolDispatcher::MakeEvent(const std::string& name)
    {
        return { { "seq", ++m_Seq }, { "type", "event" }, { "event", name } };
    }

    std::string ProtocolDispatcher::MakeEventMessage(const std::string& name, const std::string& bodyJson)
    {
        nlohmann::json event = MakeEvent(name);
        event["body"]        = nlohmann::json::parse(bodyJson, nullptr, false);
        return event.dump();
    }

    bool ProtocolDispatcher::HandleEngineCommand(const std::string& command, const nlohmann::json& arguments,
                                                 LuaDebugEngine& engine, nlohmann::json& response)
    {
        if (command == "setBreakpoints")
        {
            const std::string path = arguments.contains("source") ? arguments["source"].value("path", std::string()) : std::string();
            if (path.empty())
            {
                response["success"] = false;
                response["message"] = "setBreakpoints needs source.path";
                return true;
            }
            std::vector<int> lines;
            for (const nlohmann::json& breakpoint : arguments.value("breakpoints", nlohmann::json::array()))
                lines.push_back(breakpoint.value("line", 0));

            nlohmann::json placed = nlohmann::json::array();
            for (const BreakpointResult& result : engine.SetBreakpoints(path, lines))
            {
                nlohmann::json breakpoint = { { "verified", result.Verified }, { "line", result.Line } };
                if (!result.Message.empty())
                    breakpoint["message"] = result.Message;
                placed.push_back(std::move(breakpoint));
            }
            response["body"] = { { "breakpoints", std::move(placed) } };
            return true;
        }
        if (command == "stackTrace")
        {
            if (!engine.IsStopped())
            {
                response["success"] = false;
                response["message"] = "the script is not stopped";
                return true;
            }
            const std::vector<StackFrameInfo> frames = engine.GetStackTrace();
            const size_t start  = std::min(frames.size(), static_cast<size_t>(std::max(0, arguments.value("startFrame", 0))));
            const int    levels = arguments.value("levels", 0);
            const size_t end    = levels > 0 ? std::min(frames.size(), start + static_cast<size_t>(levels)) : frames.size();

            nlohmann::json list = nlohmann::json::array();
            for (size_t i = start; i < end; ++i)
            {
                const StackFrameInfo& frame = frames[i];
                nlohmann::json        entry = { { "id", frame.Id }, { "name", frame.Name }, { "line", frame.Line }, { "column", 1 } };
                if (!frame.Path.empty())
                    entry["source"] = { { "name", std::filesystem::path(frame.Path).filename().string() }, { "path", frame.Path } };
                else
                    entry["presentationHint"] = "subtle";
                list.push_back(std::move(entry));
            }
            response["body"] = { { "stackFrames", std::move(list) }, { "totalFrames", frames.size() } };
            return true;
        }
        if (command == "scopes")
        {
            const int frame  = arguments.value("frameId", 0);
            response["body"] = { { "scopes", nlohmann::json::array({
                                                 { { "name", "Locals" }, { "variablesReference", frame * SCOPES_PER_FRAME }, { "expensive", false } },
                                                 { { "name", "Upvalues" }, { "variablesReference", frame * SCOPES_PER_FRAME + 1 }, { "expensive", false } },
                                             }) } };
            return true;
        }
        if (command == "continue")
        {
            engine.Continue();
            response["body"] = { { "allThreadsContinued", true } };
            return true;
        }
        if (command == "pause")
        {
            engine.RequestPause();
            return true;
        }
        return false;
    }

    std::vector<std::string> ProtocolDispatcher::Handle(const std::string& body, LuaDebugEngine* engine)
    {
        const nlohmann::json request = nlohmann::json::parse(body, nullptr, false);
        if (request.is_discarded() || !request.is_object())
        {
            LOGERROR("[LuaDebug] Ignoring a message that is not a JSON object.");
            return {};
        }
        if (request.value("type", std::string()) != "request" || !request.contains("command") ||
            !request["command"].is_string())
        {
            LOGWARNING("[LuaDebug] Ignoring a message that is not a request.");
            return {};
        }

        const std::string        command  = request["command"].get<std::string>();
        nlohmann::json           response = MakeResponse(request, true);
        std::vector<std::string> out;
        if (command == "initialize")
        {
            response["body"] = MakeCapabilities();
            out.push_back(response.dump());
            out.push_back(MakeEvent("initialized").dump());
            return out;
        }
        if (command == "attach")
        {
            m_Attached = true;
        }
        else if (command == "configurationDone")
        {
            m_Configured = true;
        }
        else if (command == "threads")
        {
            response["body"] = { { "threads", nlohmann::json::array({ { { "id", LUA_THREAD_ID }, { "name", LUA_THREAD_NAME } } }) } };
        }
        else if (command == "disconnect")
        {
            m_WantsDisconnect = true;
        }
        else if (!engine || !HandleEngineCommand(command, request.value("arguments", nlohmann::json::object()), *engine, response))
        {
            response["success"] = false;
            response["message"] = "unknown command '" + command + "'";
            LOGWARNING("[LuaDebug] The debugger sent '" + command + "', which this server does not handle.");
        }
        out.push_back(response.dump());
        return out;
    }

    bool ProtocolDispatcher::IsAttached() const { return m_Attached; }

    bool ProtocolDispatcher::IsConfigured() const { return m_Configured; }

    bool ProtocolDispatcher::WantsDisconnect() const { return m_WantsDisconnect; }

    void ProtocolDispatcher::Reset() { *this = ProtocolDispatcher(); }
}
