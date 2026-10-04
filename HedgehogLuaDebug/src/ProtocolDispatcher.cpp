#include "ProtocolDispatcher.hpp"

#include "Logger/api/Logger.hpp"

namespace LuaDebug
{
    namespace
    {
        // The one thread the debugger reports: scripts run on the game thread.
        constexpr int         LUA_THREAD_ID   = 1;
        constexpr const char* LUA_THREAD_NAME = "Lua";
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

    std::vector<std::string> ProtocolDispatcher::Handle(const std::string& body)
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
        else
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
