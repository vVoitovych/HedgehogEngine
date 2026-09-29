#pragma once

#include <filesystem>
#include <fstream>
#include <string_view>

namespace EngineLogger
{
    // This run's log file: <exe directory>/Logs/<exe name>_<YYYY-MM-DD>_<HH-MM-SS>.txt.
    // A file that cannot be opened leaves the log console-only.
    class LogFile
    {
    public:
        LogFile();

        [[nodiscard]] bool IsOpen() const { return m_Stream.is_open(); }
        [[nodiscard]] const std::filesystem::path& GetPath() const { return m_Path; }

        // Writes one line, prefixed with the local time, and flushes it.
        void WriteLine(std::string_view prefix, std::string_view message);

    private:
        void Open();

        std::filesystem::path m_Path;
        std::ofstream         m_Stream;
    };
}
