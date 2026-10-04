#pragma once

#include "HedgehogLuaDebug/api/Transport.hpp"

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace LuaDebug
{
    // The debugger's TCP listener: Winsock on a thread of its own that only accepts, reads and
    // writes framed messages into the queues. It binds to the loopback address alone: Start
    // refuses any other address, so a debug build never opens a port to the network. One client
    // at a time: a second connection while one is attached is closed at once.
    class TcpTransport : public ITransport
    {
    public:
        static constexpr const char* LOOPBACK_ADDRESS = "127.0.0.1";

        // Port 0 takes any free port (GetPort tells which); bindAddress must be 127.0.0.1.
        explicit TcpTransport(uint16_t port, std::string bindAddress = LOOPBACK_ADDRESS);
        ~TcpTransport() override;

        TcpTransport(const TcpTransport&)            = delete;
        TcpTransport& operator=(const TcpTransport&) = delete;

        bool Start() override;
        void Stop() override;
        [[nodiscard]] std::vector<std::string> Receive() override;
        void Send(std::string body) override;
        [[nodiscard]] bool IsConnected() const override;
        void Disconnect() override;

        // The port listened on (the chosen one for port 0), 0 when not listening.
        [[nodiscard]] uint16_t GetPort() const;
        // Connections closed because a client was already attached.
        [[nodiscard]] uint32_t GetRejectedCount() const;

    private:
        void Run();

    private:
        uint16_t    m_RequestedPort;
        std::string m_BindAddress;

        // SOCKETs as integers, so the header does not include Winsock.
        uintptr_t                 m_Listener = ~uintptr_t{ 0 };
        std::thread               m_Thread;
        std::atomic<bool>         m_Running{ false };
        std::atomic<bool>         m_Connected{ false };
        std::atomic<bool>         m_DropClient{ false };
        std::atomic<uint16_t>     m_Port{ 0 };
        std::atomic<uint32_t>     m_Rejected{ 0 };
        bool                      m_WinsockStarted = false;

        mutable std::mutex       m_Mutex;
        std::deque<std::string>  m_Incoming;
        std::deque<std::string>  m_Outgoing;
    };
}
