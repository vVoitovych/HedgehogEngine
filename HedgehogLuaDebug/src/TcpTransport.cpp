#include "HedgehogLuaDebug/api/TcpTransport.hpp"
#include "HedgehogLuaDebug/api/MessageFraming.hpp"

#include "Logger/api/Logger.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>

#include <array>
#include <utility>

namespace LuaDebug
{
    namespace
    {
        // How long the network thread waits for a socket before checking for queued output and Stop.
        constexpr long POLL_MICROSECONDS = 10'000;
        constexpr int  LISTEN_BACKLOG    = 4;

        SOCKET ToSocket(uintptr_t value) { return static_cast<SOCKET>(value); }

        bool SendAll(SOCKET socket, const std::string& bytes)
        {
            size_t sent = 0;
            while (sent < bytes.size())
            {
                const int result = send(socket, bytes.data() + sent, static_cast<int>(bytes.size() - sent), 0);
                if (result == SOCKET_ERROR)
                    return false;
                sent += static_cast<size_t>(result);
            }
            return true;
        }
    }

    TcpTransport::TcpTransport(uint16_t port, std::string bindAddress)
        : m_RequestedPort(port)
        , m_BindAddress(std::move(bindAddress))
    {
    }

    TcpTransport::~TcpTransport() { Stop(); }

    bool TcpTransport::Start()
    {
        if (m_BindAddress != LOOPBACK_ADDRESS)
        {
            LOGERROR("[LuaDebug] Refusing to listen on " + m_BindAddress + ": the debugger binds to " + LOOPBACK_ADDRESS + " only.");
            return false;
        }
        if (m_Running)
        {
            LOGERROR("[LuaDebug] The debugger is already listening on port " + std::to_string(m_Port.load()) + ".");
            return false;
        }

        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
        {
            LOGERROR("[LuaDebug] Winsock could not start.");
            return false;
        }
        m_WinsockStarted = true;

        const SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in  address{};
        address.sin_family      = AF_INET;
        address.sin_port        = htons(m_RequestedPort);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        BOOL exclusive          = TRUE;
        if (listener == INVALID_SOCKET ||
            setsockopt(listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) != 0 ||
            bind(listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0 ||
            listen(listener, LISTEN_BACKLOG) != 0)
        {
            LOGERROR(std::string("[LuaDebug] Cannot listen on ") + LOOPBACK_ADDRESS + ":" + std::to_string(m_RequestedPort) +
                     " (Winsock error " + std::to_string(WSAGetLastError()) + ").");
            if (listener != INVALID_SOCKET)
                closesocket(listener);
            WSACleanup();
            m_WinsockStarted = false;
            return false;
        }

        sockaddr_in bound{};
        int         boundSize = sizeof(bound);
        getsockname(listener, reinterpret_cast<sockaddr*>(&bound), &boundSize);
        m_Port     = ntohs(bound.sin_port);
        m_Listener = static_cast<uintptr_t>(listener);
        m_Running  = true;
        m_Thread   = std::thread(&TcpTransport::Run, this);
        LOGINFO(std::string("[LuaDebug] Listening for the debugger on ") + LOOPBACK_ADDRESS + ":" + std::to_string(m_Port.load()) + ".");
        return true;
    }

    void TcpTransport::Stop()
    {
        if (m_Running.exchange(false) && m_Thread.joinable())
            m_Thread.join();
        if (m_Listener != ~uintptr_t{ 0 })
        {
            closesocket(ToSocket(m_Listener));
            m_Listener = ~uintptr_t{ 0 };
        }
        if (m_WinsockStarted)
        {
            WSACleanup();
            m_WinsockStarted = false;
        }
        m_Port      = 0;
        m_Connected = false;
        std::lock_guard lock(m_Mutex);
        m_Incoming.clear();
        m_Outgoing.clear();
    }

    std::vector<std::string> TcpTransport::Receive()
    {
        std::lock_guard          lock(m_Mutex);
        std::vector<std::string> messages(std::make_move_iterator(m_Incoming.begin()), std::make_move_iterator(m_Incoming.end()));
        m_Incoming.clear();
        return messages;
    }

    void TcpTransport::Send(std::string body)
    {
        if (!m_Connected)
            return;
        std::lock_guard lock(m_Mutex);
        m_Outgoing.push_back(std::move(body));
    }

    bool TcpTransport::IsConnected() const { return m_Connected; }

    void TcpTransport::Disconnect()
    {
        if (m_Connected)
            m_DropClient = true;
    }

    uint16_t TcpTransport::GetPort() const { return m_Port; }

    uint32_t TcpTransport::GetRejectedCount() const { return m_Rejected; }

    void TcpTransport::Run()
    {
        const SOCKET               listener = ToSocket(m_Listener);
        SOCKET                     client   = INVALID_SOCKET;
        MessageReader              reader;
        std::array<char, 64 * 1024> buffer{};

        const auto dropClient = [&](const char* why)
        {
            if (client == INVALID_SOCKET)
                return;
            closesocket(client);
            client      = INVALID_SOCKET;
            m_Connected = false;
            std::lock_guard lock(m_Mutex);
            m_Outgoing.clear();
            LOGINFO(std::string("[LuaDebug] The debugger ") + why + ".");
        };

        while (m_Running)
        {
            // Queued output goes out before a requested disconnect, so a reply to the client's
            // disconnect request still reaches it.
            const bool drop = m_DropClient.exchange(false);
            if (client != INVALID_SOCKET)
            {
                std::deque<std::string> outgoing;
                {
                    std::lock_guard lock(m_Mutex);
                    outgoing.swap(m_Outgoing);
                }
                for (const std::string& body : outgoing)
                {
                    if (!SendAll(client, FrameMessage(body)))
                    {
                        dropClient("could not be written to");
                        break;
                    }
                }
            }
            if (drop)
                dropClient("was disconnected");

            fd_set readable;
            FD_ZERO(&readable);
            FD_SET(listener, &readable);
            if (client != INVALID_SOCKET)
                FD_SET(client, &readable);
            timeval timeout{ 0, POLL_MICROSECONDS };
            if (select(0, &readable, nullptr, nullptr, &timeout) == SOCKET_ERROR)
                break;

            if (FD_ISSET(listener, &readable))
            {
                const SOCKET incoming = accept(listener, nullptr, nullptr);
                if (incoming != INVALID_SOCKET && client != INVALID_SOCKET)
                {
                    closesocket(incoming);
                    ++m_Rejected;
                    LOGWARNING("[LuaDebug] A second debugger tried to attach; only one may be attached at a time.");
                }
                else if (incoming != INVALID_SOCKET)
                {
                    client = incoming;
                    reader.Reset();
                    {
                        std::lock_guard lock(m_Mutex);
                        m_Incoming.clear();
                        m_Outgoing.clear();
                    }
                    m_Connected = true;
                    LOGINFO("[LuaDebug] A debugger attached.");
                }
            }

            if (client != INVALID_SOCKET && FD_ISSET(client, &readable))
            {
                const int received = recv(client, buffer.data(), static_cast<int>(buffer.size()), 0);
                if (received <= 0)
                {
                    dropClient("disconnected");
                }
                else
                {
                    reader.Append(std::string_view(buffer.data(), static_cast<size_t>(received)));
                    while (std::optional<std::string> body = reader.Next())
                    {
                        std::lock_guard lock(m_Mutex);
                        m_Incoming.push_back(std::move(*body));
                    }
                    if (reader.HasError())
                    {
                        LOGERROR("[LuaDebug] Dropping the debugger: " + reader.GetError() + ".");
                        dropClient("was dropped");
                    }
                }
            }

        }

        if (client != INVALID_SOCKET)
            closesocket(client);
        m_Connected = false;
    }
}
