#pragma once

#include "Logger/api/Logger.hpp"

#include <iostream>
#include <sstream>
#include <string>

// Everything Logger writes to the console while alive, which is what the Editor's
// ConsolePanel captures.
class LogCapture
{
public:
    LogCapture()
    {
        EngineLogger::Logger::Instance(); // its own first message is not ours
        m_Previous = std::cout.rdbuf(m_Captured.rdbuf());
    }
    ~LogCapture() { std::cout.rdbuf(m_Previous); }

    LogCapture(const LogCapture&)            = delete;
    LogCapture& operator=(const LogCapture&) = delete;

    std::string Text() const { return m_Captured.str(); }

    int CountLines(const std::string& fragment) const
    {
        std::istringstream lines(m_Captured.str());
        std::string        line;
        int                count = 0;
        while (std::getline(lines, line))
            if (line.find(fragment) != std::string::npos)
                ++count;
        return count;
    }

private:
    std::ostringstream m_Captured;
    std::streambuf*    m_Previous = nullptr;
};
