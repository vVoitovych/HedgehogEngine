#include "doctest/doctest/doctest.h"

#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    std::filesystem::path OwnDirectory()
    {
        wchar_t buffer[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        return std::filesystem::path(buffer).parent_path();
    }

    std::set<std::filesystem::path> CrashAppLogs()
    {
        std::set<std::filesystem::path> logs;
        std::error_code error;
        for (const auto& entry : std::filesystem::directory_iterator(OwnDirectory() / "Logs", error))
        {
            if (entry.path().filename().string().starts_with("LoggerCrashApp_"))
                logs.insert(entry.path());
        }
        return logs;
    }

    struct CrashAppRun
    {
        DWORD                              ExitCode = 0;
        std::vector<std::filesystem::path> NewLogs;
    };

    // Runs LoggerCrashApp <mode> beside this executable and reports the log files it left.
    CrashAppRun RunCrashApp(const std::string& mode)
    {
        const std::set<std::filesystem::path> before = CrashAppLogs();

        std::wstring command = L"\"" + (OwnDirectory() / "LoggerCrashApp.exe").wstring() + L"\" " +
                               std::wstring(mode.begin(), mode.end());
        STARTUPINFOW        startup{ sizeof(STARTUPINFOW) };
        PROCESS_INFORMATION process{};
        REQUIRE(CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process));
        REQUIRE(WaitForSingleObject(process.hProcess, 30000) == WAIT_OBJECT_0);

        CrashAppRun run;
        GetExitCodeProcess(process.hProcess, &run.ExitCode);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);

        for (const std::filesystem::path& log : CrashAppLogs())
        {
            if (!before.contains(log))
                run.NewLogs.push_back(log);
        }
        return run;
    }

    std::string ReadAndRemove(const std::filesystem::path& path)
    {
        std::string text;
        {
            std::ifstream file(path);
            std::ostringstream content;
            content << file.rdbuf();
            text = content.str();
        }
        std::filesystem::remove(path);
        return text;
    }
}

TEST_CASE("Logger - a run that ends normally leaves no log file")
{
    const CrashAppRun run = RunCrashApp("clean");
    CHECK(run.ExitCode == 0);
    CHECK(run.NewLogs.empty());
}

TEST_CASE("Logger - a crash keeps the log file, ending with why")
{
    struct Crash
    {
        const char* Mode;
        const char* Reason;
    };
    const Crash crashes[] = {
        { "access-violation", "[CRASH]access violation (0xC0000005) at LoggerCrashApp.exe+0x" },
        { "abort", "[CRASH]abort() was called." },
        { "throw", "the crash app's exception" },
    };
    for (const Crash& crash : crashes)
    {
        CAPTURE(crash.Mode);
        const CrashAppRun run = RunCrashApp(crash.Mode);
        CHECK(run.ExitCode != 0);
        REQUIRE(run.NewLogs.size() == 1);
        const std::string log = ReadAndRemove(run.NewLogs.front());
        CHECK(log.find(std::string("LoggerCrashApp: ") + crash.Mode) != std::string::npos);
        CHECK(log.find(crash.Reason) != std::string::npos);
        CHECK(log.find("[CRASH]") == log.rfind("[CRASH]")); // the first reason only
    }
}
