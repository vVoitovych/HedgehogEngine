#pragma once

#include "tinygltf/json.hpp"

#include <string>
#include <vector>

namespace LuaDebug
{
    class LuaDebugEngine;

    // The protocol state of one client: turns each request body into the bodies to send back.
    // Pure: no I/O, no Lua; the DebugServer moves the messages.
    class ProtocolDispatcher
    {
    public:
        // The bodies to send for one received body (a response, then any events), in order.
        // The engine handles the debugging commands; without one they fail.
        [[nodiscard]] std::vector<std::string> Handle(const std::string& body, LuaDebugEngine* engine);

        // An event with a JSON object body, numbered in this session's sequence.
        [[nodiscard]] std::string MakeEventMessage(const std::string& name, const std::string& bodyJson);

        [[nodiscard]] bool IsAttached() const;
        [[nodiscard]] bool IsConfigured() const;
        // The client asked to disconnect; the server drops it once the reply is sent.
        [[nodiscard]] bool WantsDisconnect() const;

        // A new client: the session starts over (sequence numbers included).
        void Reset();

        // What initialize declares.
        [[nodiscard]] static nlohmann::json MakeCapabilities();

    private:
        [[nodiscard]] nlohmann::json MakeResponse(const nlohmann::json& request, bool success);
        [[nodiscard]] nlohmann::json MakeEvent(const std::string& name);
        // Fills response for setBreakpoints, stackTrace, scopes, continue and pause; false for
        // any other command.
        bool HandleEngineCommand(const std::string& command, const nlohmann::json& arguments, LuaDebugEngine& engine,
                                 nlohmann::json& response);

    private:
        int  m_Seq             = 0;
        bool m_Attached        = false;
        bool m_Configured      = false;
        bool m_WantsDisconnect = false;
    };
}
