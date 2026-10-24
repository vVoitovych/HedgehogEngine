#include "api/Logger.hpp"
#include "LogColorized.hpp"
#include "LogFile.hpp"

#ifdef _WIN32
#include <Windows.h>
#endif

#include <atomic>
#include <csignal>
#include <cstring>
#include <exception>
#include <format>
#include <mutex>

namespace EngineLogger
{
    namespace
    {
        constexpr uint16_t DEFAULT_COLOR = 7;

        struct LevelStyle
        {
            const char* Prefix;
            uint16_t    Color;
        };

        LevelStyle StyleOf(LogLevel level)
        {
            switch (level)
            {
            case LogLevel::Verbose: return { "[VERBOSE]", 10 };
            case LogLevel::Warning: return { "[WARNING]", 14 };
            case LogLevel::Error:   return { "[ERROR]",   12 };
            case LogLevel::Info:    break;
            }
            return { "[INFO]", DEFAULT_COLOR };
        }

        // One line at a time, on the console and in the file, whatever thread logs it.
        std::mutex& WriteMutex()
        {
            static std::mutex mutex;
            return mutex;
        }

        // ─── Crashes ─────────────────────────────────────────────────────────
        // A crash ends the process without running static destructors, so the log file stays;
        // these handlers only add why. They take no lock (the crashing thread may hold it) and
        // record the first reason only (an uncaught exception's terminate is followed by abort).

        LogFile*          g_CrashFile = nullptr;
        std::atomic<bool> g_CrashRecorded{ false };

        void RecordCrash(const std::string& reason)
        {
            if (g_CrashRecorded.exchange(true))
                return;
            std::cerr << "[CRASH]" << reason << std::endl;
            if (g_CrashFile != nullptr && g_CrashFile->IsOpen())
                g_CrashFile->WriteLine("[CRASH]", reason);
        }

        void OnAbort(int)
        {
            RecordCrash("abort() was called.");
        }

        std::terminate_handler g_PreviousTerminate = nullptr;

        void OnTerminate()
        {
            std::string reason = "std::terminate was called";
            if (const std::exception_ptr current = std::current_exception())
            {
                try
                {
                    std::rethrow_exception(current);
                }
                catch (const std::exception& exception)
                {
                    reason += std::string(" by an uncaught exception: ") + exception.what();
                }
                catch (...)
                {
                    reason += " by an uncaught exception that is not a std::exception";
                }
            }
            RecordCrash(reason + ".");
            if (g_PreviousTerminate != nullptr)
                g_PreviousTerminate();
            std::abort();
        }

#ifdef _WIN32
        LPTOP_LEVEL_EXCEPTION_FILTER g_PreviousFilter = nullptr;

        // What MSVC raises for a C++ throw ("msc").
        constexpr DWORD CPP_EXCEPTION_CODE = 0xE06D7363;

        const char* ExceptionName(DWORD code)
        {
            switch (code)
            {
            case EXCEPTION_ACCESS_VIOLATION:      return "access violation";
            case EXCEPTION_STACK_OVERFLOW:        return "stack overflow";
            case EXCEPTION_INT_DIVIDE_BY_ZERO:    return "integer division by zero";
            case EXCEPTION_ILLEGAL_INSTRUCTION:   return "illegal instruction";
            case EXCEPTION_IN_PAGE_ERROR:         return "in-page error";
            case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "array bounds exceeded";
            case EXCEPTION_PRIV_INSTRUCTION:      return "privileged instruction";
            case EXCEPTION_BREAKPOINT:            return "breakpoint";
            case CPP_EXCEPTION_CODE:              return "uncaught C++ exception";
            default:                              return "unhandled exception";
            }
        }

        // "<module>+0x<offset>", or the raw address when no module holds it.
        std::string DescribeAddress(const void* address)
        {
            HMODULE module = nullptr;
            if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   static_cast<LPCWSTR>(address), &module))
            {
                char path[MAX_PATH] = {};
                if (GetModuleFileNameA(module, path, MAX_PATH) != 0)
                {
                    const char* name = std::strrchr(path, '\\');
                    return std::format("{}+0x{:X}", name != nullptr ? name + 1 : path,
                        static_cast<const char*>(address) - reinterpret_cast<const char*>(module));
                }
            }
            return std::format("0x{:X}", reinterpret_cast<uintptr_t>(address));
        }

        LONG WINAPI OnUnhandledException(EXCEPTION_POINTERS* info)
        {
            const EXCEPTION_RECORD& record = *info->ExceptionRecord;
            // An uncaught C++ exception: the CRT's own filter calls std::terminate, whose handler
            // names the exception; its address here would only be RaiseException's.
            if (record.ExceptionCode == CPP_EXCEPTION_CODE && g_PreviousFilter != nullptr)
                return g_PreviousFilter(info);
            std::string reason = std::format("{} (0x{:08X}) at {}", ExceptionName(record.ExceptionCode),
                record.ExceptionCode, DescribeAddress(record.ExceptionAddress));
            if (record.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record.NumberParameters >= 2)
            {
                const ULONG_PTR operation = record.ExceptionInformation[0];
                reason += std::format(", {} address 0x{:X}",
                    operation == 0 ? "reading" : operation == 1 ? "writing" : "executing", record.ExceptionInformation[1]);
            }
            RecordCrash(reason + ".");
            // Windows (or a previous filter) still reports the crash as before.
            return g_PreviousFilter != nullptr ? g_PreviousFilter(info) : EXCEPTION_CONTINUE_SEARCH;
        }
#endif

        void InstallCrashHandlers(LogFile& file)
        {
            g_CrashFile = &file;
            std::signal(SIGABRT, OnAbort);
            // The terminate handler is the thread's: the one creating the logger, the main thread
            // in practice; abort covers the others.
            g_PreviousTerminate = std::set_terminate(OnTerminate);
#ifdef _WIN32
            g_PreviousFilter = SetUnhandledExceptionFilter(OnUnhandledException);
#endif
        }
    }

    Logger::Logger()
        : mColoriser(std::make_unique<LogColorized>())
        , mFile(std::make_unique<LogFile>())
    {
        InstallCrashHandlers(*mFile);
        if (mFile->IsOpen())
        {
            const std::u8string path = mFile->GetPath().u8string();
            Write(LogLevel::Info, "Logger: writing this run's log to " + std::string(path.begin(), path.end()) +
                                  "; it is kept only if the run crashes.");
        }
        else
        {
            Write(LogLevel::Warning, "Logger: could not create a log file in the Logs folder beside the executable; logging to the console only.");
        }
    }

    // Static destruction: the process is ending. abort() ends it through ExitProcess, which still
    // destroys this DLL's statics, so a recorded crash keeps the file; any other end discards it.
    Logger::~Logger()
    {
        const std::lock_guard lock(WriteMutex());
        g_CrashFile = nullptr;
        if (!g_CrashRecorded)
            mFile->Discard();
    }

    Logger& Logger::Instance()
    {
        static Logger instance;
        return instance;
    }

    int Logger::AddSink(Sink sink)
    {
        const std::lock_guard lock(WriteMutex());
        mSinks.emplace_back(mNextSinkId, std::move(sink));
        return mNextSinkId++;
    }

    void Logger::RemoveSink(int id)
    {
        const std::lock_guard lock(WriteMutex());
        std::erase_if(mSinks, [id](const auto& entry) { return entry.first == id; });
    }

    void Logger::Write(LogLevel level, const std::string& message)
    {
        const LevelStyle style = StyleOf(level);

        const std::lock_guard lock(WriteMutex());

        mColoriser->SetLogColor(style.Color);
        std::cout << style.Prefix << message << std::endl;
        mColoriser->SetLogColor(DEFAULT_COLOR);

        if (mFile->IsOpen())
            mFile->WriteLine(style.Prefix, message);

        for (const auto& [id, sink] : mSinks)
            sink(level, message);
    }
}
