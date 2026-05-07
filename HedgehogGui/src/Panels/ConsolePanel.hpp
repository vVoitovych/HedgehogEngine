#pragma once

#include <iostream>
#include <sstream>
#include <streambuf>
#include <string>
#include <vector>

namespace Editor
{
    enum class LogLevel { Info, Verbose, Warning, Error };

    struct LogEntry
    {
        LogLevel    m_Level;
        std::string m_Text;
    };

    // Captures std::cout output while still forwarding it to the real console.
    class ConsolePanel
    {
    public:
        ConsolePanel();
        ~ConsolePanel();

        ConsolePanel(const ConsolePanel&)            = delete;
        ConsolePanel& operator=(const ConsolePanel&) = delete;
        ConsolePanel(ConsolePanel&&)                 = delete;
        ConsolePanel& operator=(ConsolePanel&&)      = delete;

        void Draw();
        void Clear();

    private:
        void ParseCaptured();

        // Tees every character to both the original cout buffer and an internal buffer.
        class TeeBuf : public std::streambuf
        {
        public:
            explicit TeeBuf(std::streambuf* primary);

            std::string Consume();

        protected:
            int         overflow(int c) override;
            std::streamsize xsputn(const char* s, std::streamsize n) override;

        private:
            std::streambuf*    m_Primary;
            std::ostringstream m_Captured;
        };

    private:
        TeeBuf          m_TeeBuf;
        std::streambuf* m_OldCoutBuf;

        std::vector<LogEntry> m_Entries;
        bool                  m_ScrollToBottom = true;
    };
}
