#include "HedgehogLuaDebug/api/MessageFraming.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>

namespace LuaDebug
{
    namespace
    {
        constexpr std::string_view HEADER_END     = "\r\n\r\n";
        constexpr std::string_view LINE_END       = "\r\n";
        constexpr std::string_view CONTENT_LENGTH = "content-length:";

        bool StartsWithIgnoringCase(std::string_view text, std::string_view prefix)
        {
            return text.size() >= prefix.size() &&
                   std::equal(prefix.begin(), prefix.end(), text.begin(),
                              [](char expected, char actual) { return std::tolower(static_cast<unsigned char>(actual)) == expected; });
        }
    }

    std::string FrameMessage(std::string_view body)
    {
        std::string message = "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";
        message += body;
        return message;
    }

    void MessageReader::Append(std::string_view bytes)
    {
        if (m_Error.empty())
            m_Buffer.append(bytes);
    }

    std::optional<std::string> MessageReader::Next()
    {
        if (!m_Error.empty())
            return std::nullopt;

        const size_t headerEnd = m_Buffer.find(HEADER_END);
        if (headerEnd == std::string::npos)
        {
            if (m_Buffer.size() > MAX_HEADER_SIZE)
                m_Error = "the message header has no end within " + std::to_string(MAX_HEADER_SIZE) + " bytes";
            return std::nullopt;
        }
        if (headerEnd > MAX_HEADER_SIZE)
        {
            m_Error = "the message header is longer than " + std::to_string(MAX_HEADER_SIZE) + " bytes";
            return std::nullopt;
        }

        // Header lines in any order; only Content-Length matters.
        std::optional<size_t>  length;
        const std::string_view header(m_Buffer.data(), headerEnd);
        size_t                 start = 0;
        while (start <= header.size())
        {
            const size_t           end  = std::min(header.find(LINE_END, start), header.size());
            const std::string_view line = header.substr(start, end - start);
            if (StartsWithIgnoringCase(line, CONTENT_LENGTH))
            {
                std::string_view value = line.substr(CONTENT_LENGTH.size());
                while (!value.empty() && value.front() == ' ')
                    value.remove_prefix(1);
                size_t     parsed = 0;
                const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
                if (result.ec != std::errc() || result.ptr != value.data() + value.size() || value.empty())
                {
                    m_Error = "the Content-Length '" + std::string(value) + "' is not a number";
                    return std::nullopt;
                }
                length = parsed;
            }
            start = end + LINE_END.size();
        }
        if (!length)
        {
            m_Error = "the message header has no Content-Length";
            return std::nullopt;
        }
        if (*length > MAX_BODY_SIZE)
        {
            m_Error = "the message body of " + std::to_string(*length) + " bytes is too large";
            return std::nullopt;
        }

        const size_t bodyStart = headerEnd + HEADER_END.size();
        if (m_Buffer.size() - bodyStart < *length)
            return std::nullopt;
        std::string body = m_Buffer.substr(bodyStart, *length);
        m_Buffer.erase(0, bodyStart + *length);
        return body;
    }

    bool MessageReader::HasError() const { return !m_Error.empty(); }

    const std::string& MessageReader::GetError() const { return m_Error; }

    void MessageReader::Reset()
    {
        m_Buffer.clear();
        m_Error.clear();
    }
}
