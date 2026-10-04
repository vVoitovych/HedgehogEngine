#pragma once

#include "HedgehogLuaDebug/api/Transport.hpp"

#include <deque>
#include <memory>
#include <string>

namespace LuaDebug
{
    class LuaDebugEngine;
    class ProtocolDispatcher;

    // The Lua debugger's Debug Adapter Protocol server (epic HE-180). It owns a transport (TCP on
    // the loopback address, or an in-memory one in tests) and handles the client's requests only
    // when the engine calls Pump on the game thread, so no Lua state is ever touched off-thread.
    //
    // Requests handled: initialize (the declared capabilities, then the initialized event),
    // attach, configurationDone, threads (one, "Lua") and disconnect (replied to, then the client
    // is dropped), and, through the LuaDebugEngine registered with it, setBreakpoints, stackTrace,
    // scopes, continue and pause. Any other request gets success: false with a message naming the
    // command; a message that is not JSON or not a request is logged and ignored. When the client
    // leaves, the engine's session is cleared (no breakpoints, no stop).
    class DebugServer
    {
    public:
        explicit DebugServer(std::unique_ptr<ITransport> transport);
        ~DebugServer();

        DebugServer(const DebugServer&)            = delete;
        DebugServer& operator=(const DebugServer&) = delete;

        // Starts the transport; false, logged, when it cannot.
        bool Start();
        // Drops any client and stops listening. Safe twice.
        void Stop();

        // Handles the requests received since the last call, in order, once a frame. A request
        // that resumes a stopped script (continue, a step) ends the call: what follows it waits
        // for the next one, so it reaches the next stop rather than this one.
        void Pump();

        // A client sent attach and has not left.
        [[nodiscard]] bool IsAttached() const;
        // The attached client sent configurationDone (its breakpoints are set).
        [[nodiscard]] bool IsConfigured() const;

        [[nodiscard]] ITransport& GetTransport();

        // The engine debugging the VM (it registers itself; nullptr when it goes).
        void SetEngine(LuaDebugEngine* engine);
        // Sends an event with a JSON object body, numbered in the session's sequence.
        void SendEvent(const std::string& event, const std::string& bodyJson);

    private:
        std::unique_ptr<ITransport>         m_Transport;
        std::unique_ptr<ProtocolDispatcher> m_Dispatcher;
        LuaDebugEngine*                     m_Engine = nullptr;
        std::deque<std::string>             m_Pending;
    };
}
