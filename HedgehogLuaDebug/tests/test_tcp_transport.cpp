#include "doctest/doctest/doctest.h"

#include "HedgehogLuaDebug/api/MessageFraming.hpp"
#include "HedgehogLuaDebug/api/TcpTransport.hpp"

#include "HedgehogScripting/tests/test_log_capture.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace
{
    // Winsock for the test's own client sockets.
    struct Winsock
    {
        Winsock()
        {
            WSADATA data{};
            REQUIRE(WSAStartup(MAKEWORD(2, 2), &data) == 0);
        }
        ~Winsock() { WSACleanup(); }
    };

    bool WaitFor(const std::function<bool()>& condition)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (condition())
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return condition();
    }

    SOCKET Connect(uint16_t port)
    {
        const SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in  address{};
        address.sin_family      = AF_INET;
        address.sin_port        = htons(port);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        REQUIRE(connect(client, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0);
        return client;
    }

    // Reads until one framed message arrives, or the peer closes (nullopt).
    std::optional<std::string> ReadMessage(SOCKET socket, LuaDebug::MessageReader& reader)
    {
        DWORD timeout = 5000;
        setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        char buffer[4096];
        while (true)
        {
            if (std::optional<std::string> body = reader.Next())
                return body;
            const int received = recv(socket, buffer, sizeof(buffer), 0);
            if (received <= 0)
                return std::nullopt;
            reader.Append(std::string_view(buffer, static_cast<size_t>(received)));
        }
    }
}

TEST_CASE("TCP transport - only the loopback address can be bound")
{
    LogCapture log;
    for (const char* address : { "0.0.0.0", "192.168.1.10", "localhost", "::1", "" })
    {
        CAPTURE(address);
        LuaDebug::TcpTransport transport(0, address);
        CHECK_FALSE(transport.Start());
        CHECK(transport.GetPort() == 0);
    }
    CHECK(log.Lines("binds to 127.0.0.1 only").size() == 5);
}

TEST_CASE("TCP transport - messages travel both ways framed, split or merged on the wire")
{
    Winsock                winsock;
    LuaDebug::TcpTransport transport(0);
    REQUIRE(transport.Start());
    REQUIRE(transport.GetPort() != 0);
    CHECK_FALSE(transport.IsConnected());

    const SOCKET client = Connect(transport.GetPort());
    REQUIRE(WaitFor([&] { return transport.IsConnected(); }));

    // Two messages in one write, then one in two writes.
    const std::string both = LuaDebug::FrameMessage("{\"seq\":1}") + LuaDebug::FrameMessage("{\"seq\":2}");
    REQUIRE(send(client, both.data(), static_cast<int>(both.size()), 0) == static_cast<int>(both.size()));
    const std::string third = LuaDebug::FrameMessage("{\"seq\":3}");
    REQUIRE(send(client, third.data(), 5, 0) == 5);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    REQUIRE(send(client, third.data() + 5, static_cast<int>(third.size() - 5), 0) == static_cast<int>(third.size() - 5));

    std::vector<std::string> received;
    REQUIRE(WaitFor([&]
                    {
                        for (std::string& body : transport.Receive())
                            received.push_back(std::move(body));
                        return received.size() >= 3;
                    }));
    CHECK(received == std::vector<std::string>{ "{\"seq\":1}", "{\"seq\":2}", "{\"seq\":3}" });

    transport.Send("{\"type\":\"response\"}");
    LuaDebug::MessageReader reader;
    CHECK(ReadMessage(client, reader) == "{\"type\":\"response\"}");

    // A requested disconnect still delivers what was queued before it, then closes.
    transport.Send("{\"type\":\"bye\"}");
    transport.Disconnect();
    CHECK(ReadMessage(client, reader) == "{\"type\":\"bye\"}");
    CHECK_FALSE(ReadMessage(client, reader));
    CHECK(WaitFor([&] { return !transport.IsConnected(); }));

    closesocket(client);
    transport.Stop();
    CHECK(transport.GetPort() == 0);
}

TEST_CASE("TCP transport - a second client is rejected while one is attached")
{
    Winsock                winsock;
    LuaDebug::TcpTransport transport(0);
    REQUIRE(transport.Start());

    const SOCKET first = Connect(transport.GetPort());
    REQUIRE(WaitFor([&] { return transport.IsConnected(); }));

    const SOCKET second = Connect(transport.GetPort());
    REQUIRE(WaitFor([&] { return transport.GetRejectedCount() == 1; }));
    LuaDebug::MessageReader reader;
    CHECK_FALSE(ReadMessage(second, reader)); // closed by the server
    closesocket(second);
    CHECK(transport.IsConnected());

    // Once the first leaves, a new client attaches.
    closesocket(first);
    REQUIRE(WaitFor([&] { return !transport.IsConnected(); }));
    const SOCKET third = Connect(transport.GetPort());
    CHECK(WaitFor([&] { return transport.IsConnected(); }));
    closesocket(third);
    transport.Stop();
    transport.Stop();
}
