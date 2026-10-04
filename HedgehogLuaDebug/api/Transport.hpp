#pragma once

#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace LuaDebug
{
    // Moves DAP message bodies (JSON text, unframed) between a debugger client and the game
    // thread. Implementations may do their I/O on a thread of their own, but every method here is
    // safe to call from the game thread at any time: the transport only queues, and all protocol
    // handling happens when the engine pumps the DebugServer.
    class ITransport
    {
    public:
        virtual ~ITransport() = default;

        // Starts listening; false, logged, when it cannot.
        virtual bool Start() = 0;
        // Disconnects any client and stops listening. Safe twice and before Start.
        virtual void Stop() = 0;

        // The message bodies received since the last call, in order.
        [[nodiscard]] virtual std::vector<std::string> Receive() = 0;
        // Queues a body for the client; dropped when none is attached.
        virtual void Send(std::string body) = 0;

        // A client is attached.
        [[nodiscard]] virtual bool IsConnected() const = 0;
        // Drops the attached client, if any; the transport keeps listening for the next one.
        virtual void Disconnect() = 0;
    };

    // A transport with no I/O, for tests: the test pushes the client's messages in and takes the
    // server's out. It is connected from Start until Stop or Disconnect.
    class InMemoryTransport : public ITransport
    {
    public:
        bool Start() override;
        void Stop() override;
        [[nodiscard]] std::vector<std::string> Receive() override;
        void Send(std::string body) override;
        [[nodiscard]] bool IsConnected() const override;
        void Disconnect() override;

        // The client's side.
        void PushFromClient(std::string body);
        [[nodiscard]] std::vector<std::string> TakeSent();
        // A client that reacts: called with each body the server sends (after it is recorded,
        // outside the lock, so it may push replies).
        void SetClient(std::function<void(const std::string& body)> client);

    private:
        mutable std::mutex       m_Mutex;
        std::deque<std::string>  m_Incoming;
        std::vector<std::string> m_Outgoing;
        std::function<void(const std::string&)> m_Client;
        bool                     m_Connected = false;
    };
}
