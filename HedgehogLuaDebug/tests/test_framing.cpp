#include "doctest/doctest/doctest.h"

#include "HedgehogLuaDebug/api/MessageFraming.hpp"

#include <string>

using LuaDebug::FrameMessage;
using LuaDebug::MessageReader;

TEST_CASE("Framing - a framed message reads back as its body")
{
    CHECK(FrameMessage(R"({"seq":1})") == "Content-Length: 9\r\n\r\n{\"seq\":1}");

    MessageReader reader;
    reader.Append(FrameMessage(R"({"seq":1})"));
    CHECK(reader.Next() == R"({"seq":1})");
    CHECK_FALSE(reader.Next());
    CHECK_FALSE(reader.HasError());
}

TEST_CASE("Framing - a message split across reads waits for the rest, byte by byte included")
{
    const std::string body    = R"({"command":"initialize","arguments":{"adapterID":"hedgehog-lua"}})";
    const std::string message = FrameMessage(body);

    MessageReader reader;
    for (size_t i = 0; i + 1 < message.size(); ++i)
    {
        reader.Append(std::string_view(&message[i], 1));
        CHECK_FALSE(reader.Next());
    }
    reader.Append(std::string_view(&message.back(), 1));
    CHECK(reader.Next() == body);

    // Split inside the header and inside the body.
    reader.Append(message.substr(0, 7));
    CHECK_FALSE(reader.Next());
    reader.Append(message.substr(7, 20));
    CHECK_FALSE(reader.Next());
    reader.Append(message.substr(27));
    CHECK(reader.Next() == body);
}

TEST_CASE("Framing - several messages in one read come out one at a time, in order")
{
    MessageReader reader;
    reader.Append(FrameMessage("{\"a\":1}") + FrameMessage("{}") + FrameMessage("{\"b\":2}").substr(0, 10));
    CHECK(reader.Next() == "{\"a\":1}");
    CHECK(reader.Next() == "{}");
    CHECK_FALSE(reader.Next());
    reader.Append(FrameMessage("{\"b\":2}").substr(10));
    CHECK(reader.Next() == "{\"b\":2}");

    // Other header lines and any case of Content-Length are accepted; an empty body is a message.
    reader.Append("Content-Type: application/vscode-jsonrpc; charset=utf-8\r\ncontent-length: 2\r\n\r\n{}");
    CHECK(reader.Next() == "{}");
    reader.Append("Content-Length: 0\r\n\r\n");
    CHECK(reader.Next() == "");
}

TEST_CASE("Framing - a malformed header stops the reader with an error")
{
    MessageReader missing;
    missing.Append("Content-Type: x\r\n\r\n{}");
    CHECK_FALSE(missing.Next());
    CHECK(missing.GetError() == "the message header has no Content-Length");

    MessageReader notNumber;
    notNumber.Append("Content-Length: 12abc\r\n\r\n");
    CHECK_FALSE(notNumber.Next());
    CHECK(notNumber.GetError() == "the Content-Length '12abc' is not a number");

    MessageReader tooLong;
    tooLong.Append(std::string(MessageReader::MAX_HEADER_SIZE + 1, 'x'));
    CHECK_FALSE(tooLong.Next());
    CHECK(tooLong.HasError());

    MessageReader tooLarge;
    tooLarge.Append("Content-Length: 999999999\r\n\r\n");
    CHECK_FALSE(tooLarge.Next());
    CHECK(tooLarge.HasError());

    // Once in error nothing more is read until Reset.
    tooLarge.Append(FrameMessage("{}"));
    CHECK_FALSE(tooLarge.Next());
    tooLarge.Reset();
    tooLarge.Append(FrameMessage("{}"));
    CHECK(tooLarge.Next() == "{}");
}
