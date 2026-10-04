#pragma once

#include "HedgehogLuaDebug/api/Transport.hpp"

#include <memory>

namespace LuaDebug
{
    class ProtocolDispatcher;

    // The Lua debugger's Debug Adapter Protocol server (epic HE-180). It owns a transport (TCP on
    // the loopback address, or an in-memory one in tests) and handles the client's requests only
    // when the engine calls Pump on the game thread, so no Lua state is ever touched off-thread.
    //
    // Requests handled: initialize (the declared capabilities, then the initialized event),
    // attach, configurationDone, threads (one, "Lua") and disconnect (replied to, then the client
    // is dropped). Any other request gets success: false with a message naming the command; a
    // message that is not JSON or not a request is logged and ignored.
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

        // Handles every request received since the last call, in order. Call once a frame.
        void Pump();

        // A client sent attach and has not left.
        [[nodiscard]] bool IsAttached() const;
        // The attached client sent configurationDone (its breakpoints are set).
        [[nodiscard]] bool IsConfigured() const;

        [[nodiscard]] ITransport& GetTransport();

    private:
        std::unique_ptr<ITransport>         m_Transport;
        std::unique_ptr<ProtocolDispatcher> m_Dispatcher;
    };
}
