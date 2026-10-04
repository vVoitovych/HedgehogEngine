#include "api/Logger.hpp"
#include "LogColorized.hpp"
#include "LogFile.hpp"

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
    }

    Logger::Logger()
        : mColoriser(std::make_unique<LogColorized>())
        , mFile(std::make_unique<LogFile>())
    {
        if (mFile->IsOpen())
        {
            const std::u8string path = mFile->GetPath().u8string();
            Write(LogLevel::Info, "Logger: writing this run's log to " + std::string(path.begin(), path.end()));
        }
        else
        {
            Write(LogLevel::Warning, "Logger: could not create a log file in the Logs folder beside the executable; logging to the console only.");
        }
    }

    Logger::~Logger() = default;

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
