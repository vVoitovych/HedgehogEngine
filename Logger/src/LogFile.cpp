#include "LogFile.hpp"

#ifdef _WIN32
#include <Windows.h>
#endif

#include <chrono>
#include <ctime>
#include <format>
#include <string>
#include <system_error>
#include <vector>

namespace EngineLogger
{
    namespace
    {
        constexpr const char* LOGS_FOLDER = "Logs";

        std::filesystem::path ExecutablePath()
        {
#ifdef _WIN32
            std::vector<wchar_t> buffer(MAX_PATH);
            for (;;)
            {
                const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
                if (length == 0)
                    return {};
                if (length < buffer.size())
                    return std::filesystem::path(std::wstring(buffer.data(), length));
                buffer.resize(buffer.size() * 2);
            }
#else
            std::error_code error;
            return std::filesystem::read_symlink("/proc/self/exe", error);
#endif
        }

        std::tm LocalTime(std::time_t time)
        {
            std::tm local{};
#ifdef _WIN32
            localtime_s(&local, &time);
#else
            localtime_r(&time, &local);
#endif
            return local;
        }
    }

    LogFile::LogFile()
    {
        // The logger must never throw: any failure below just leaves the file closed.
        try
        {
            Open();
        }
        catch (...)
        {
            m_Stream.close();
            m_Path.clear();
        }
    }

    void LogFile::Open()
    {
        const std::filesystem::path executable = ExecutablePath();
        if (executable.empty())
            return;

        const std::filesystem::path folder = executable.parent_path() / LOGS_FOLDER;
        std::error_code error;
        std::filesystem::create_directories(folder, error);
        if (error)
            return;

        const std::tm     now  = LocalTime(std::time(nullptr));
        const std::string stem = std::format("{}_{:04}-{:02}-{:02}_{:02}-{:02}-{:02}",
            executable.stem().string(), now.tm_year + 1900, now.tm_mon + 1, now.tm_mday,
            now.tm_hour, now.tm_min, now.tm_sec);

        // Two runs started in the same second must not share a file.
        std::filesystem::path path = folder / (stem + ".txt");
        for (int suffix = 2; std::filesystem::exists(path, error); ++suffix)
            path = folder / std::format("{}_{}.txt", stem, suffix);

        m_Stream.open(path, std::ios::out | std::ios::trunc);
        if (m_Stream.is_open())
            m_Path = path;
    }

    void LogFile::WriteLine(std::string_view prefix, std::string_view message)
    {
        const auto now    = std::chrono::system_clock::now();
        const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
        const std::tm local = LocalTime(std::chrono::system_clock::to_time_t(now));

        m_Stream << std::format("[{:02}:{:02}:{:02}.{:03}]", local.tm_hour, local.tm_min, local.tm_sec, millis)
                 << prefix << message << '\n';
        m_Stream.flush();
    }
}
