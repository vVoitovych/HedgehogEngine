#include "doctest/doctest/doctest.h"

#include "HedgehogLuaDebug/api/DebugServer.hpp"

#include "HedgehogScripting/tests/test_log_capture.hpp"

#include "tinygltf/json.hpp"

#include <memory>
#include <string>
#include <vector>

using nlohmann::json;

namespace
{
    // A server over an in-memory transport, with the client's side at hand.
    struct Session
    {
        LuaDebug::InMemoryTransport* Client = nullptr;
        LuaDebug::DebugServer        Server;

        Session()
            : Server(MakeTransport(Client))
        {
            REQUIRE(Server.Start());
        }

        static std::unique_ptr<LuaDebug::ITransport> MakeTransport(LuaDebug::InMemoryTransport*& client)
        {
            auto transport = std::make_unique<LuaDebug::InMemoryTransport>();
            client         = transport.get();
            return transport;
        }

        // Sends one request and pumps; returns what the server sent back.
        std::vector<json> Request(int seq, const std::string& command, json arguments = json::object())
        {
            Client->PushFromClient(json{ { "seq", seq }, { "type", "request" }, { "command", command }, { "arguments", arguments } }.dump());
            Server.Pump();
            std::vector<json> replies;
            for (const std::string& body : Client->TakeSent())
                replies.push_back(json::parse(body));
            return replies;
        }
    };
}

TEST_CASE("Debug server - initialize returns the declared capabilities, then the initialized event")
{
    Session                 session;
    const std::vector<json> replies = session.Request(1, "initialize", { { "adapterID", "hedgehog-lua" } });
    REQUIRE(replies.size() == 2);

    const json& response = replies[0];
    CHECK(response["type"] == "response");
    CHECK(response["request_seq"] == 1);
    CHECK(response["command"] == "initialize");
    CHECK(response["success"] == true);
    CHECK(response["body"]["supportsConfigurationDoneRequest"] == true);
    CHECK(response["body"]["supportsConditionalBreakpoints"] == false);
    CHECK(response["body"]["supportsStepBack"] == false);

    CHECK(replies[1]["type"] == "event");
    CHECK(replies[1]["event"] == "initialized");
    CHECK(replies[1]["seq"].get<int>() == response["seq"].get<int>() + 1);
}

TEST_CASE("Debug server - attach, configurationDone, threads and disconnect")
{
    Session session;
    (void)session.Request(1, "initialize");
    CHECK_FALSE(session.Server.IsAttached());

    std::vector<json> replies = session.Request(2, "attach");
    REQUIRE(replies.size() == 1);
    CHECK(replies[0]["success"] == true);
    CHECK(session.Server.IsAttached());
    CHECK_FALSE(session.Server.IsConfigured());

    replies = session.Request(3, "configurationDone");
    CHECK(replies[0]["success"] == true);
    CHECK(session.Server.IsConfigured());

    replies = session.Request(4, "threads");
    REQUIRE(replies.size() == 1);
    CHECK(replies[0]["body"]["threads"] == json::array({ { { "id", 1 }, { "name", "Lua" } } }));

    // The reply is sent, then the client is dropped and the session starts over.
    session.Client->PushFromClient(json{ { "seq", 5 }, { "type", "request" }, { "command", "disconnect" } }.dump());
    session.Server.Pump();
    CHECK_FALSE(session.Client->IsConnected());
    CHECK_FALSE(session.Server.IsAttached());
    CHECK_FALSE(session.Server.IsConfigured());
}

TEST_CASE("Debug server - an unknown request fails naming the command; non-requests are ignored")
{
    Session    session;
    LogCapture log;

    std::vector<json> replies = session.Request(7, "stackTrace", { { "threadId", 1 } });
    REQUIRE(replies.size() == 1);
    CHECK(replies[0]["success"] == false);
    CHECK(replies[0]["command"] == "stackTrace");
    CHECK(replies[0]["request_seq"] == 7);
    CHECK(replies[0]["message"] == "unknown command 'stackTrace'");
    CHECK(log.Lines("does not handle").size() == 1);

    session.Client->PushFromClient("not json");
    session.Client->PushFromClient(R"({"seq":8,"type":"event","event":"x"})");
    session.Client->PushFromClient(R"({"seq":9,"type":"request"})");
    session.Server.Pump();
    CHECK(session.Client->TakeSent().empty());
    CHECK(log.Lines("not a JSON object").size() == 1);
    CHECK(log.Lines("not a request").size() == 2);

    // The session carries on.
    CHECK(session.Request(10, "threads").size() == 1);
}

TEST_CASE("Debug server - a client that leaves without disconnect starts the next session afresh")
{
    Session session;
    (void)session.Request(1, "attach");
    CHECK(session.Server.IsAttached());

    session.Client->Disconnect();
    session.Server.Pump();
    CHECK_FALSE(session.Server.IsAttached());

    session.Server.Stop();
    session.Server.Stop();
}
