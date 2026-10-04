#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace LuaDebug
{
    // A Debug Adapter Protocol message on the wire: "Content-Length: <n>\r\n", optional other
    // header lines, "\r\n", then n bytes of JSON.
    [[nodiscard]] std::string FrameMessage(std::string_view body);

    // Splits a byte stream into message bodies, whatever the reads it arrives in: a message split
    // across reads waits for the rest, and several in one read come out one at a time.
    class MessageReader
    {
    public:
        // A header block longer than this, or a body larger than MAX_BODY_SIZE, is an error.
        static constexpr size_t MAX_HEADER_SIZE = 1024;
        static constexpr size_t MAX_BODY_SIZE   = 16 * 1024 * 1024;

        void Append(std::string_view bytes);

        // The next complete body, or nullopt until one has arrived (or after an error).
        [[nodiscard]] std::optional<std::string> Next();

        // A malformed header (no Content-Length, a bad number, too long): the stream cannot be
        // resynchronized, so the reader stops; the connection should be dropped.
        [[nodiscard]] bool HasError() const;
        [[nodiscard]] const std::string& GetError() const;

        // Forgets everything, error included (a new connection).
        void Reset();

    private:
        std::string m_Buffer;
        std::string m_Error;
    };
}
