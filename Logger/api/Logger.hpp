#pragma once

#ifdef _WIN32
#define NOMINMAX
#endif

#include <iostream>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include "LoggerApi.hpp"

namespace EngineLogger
{
    class LogColorized;
    class LogFile;

    enum class LogLevel : uint8_t
    {
        Info,
        Verbose,
        Warning,
        Error
    };

    // Writes each message to the console and to this run's file, Logs/<exe>_<date>_<time>.txt
    // beside the executable.
    class Logger
    {
    public:
        LOGGER_API static Logger& Instance();

    private:
        LOGGER_API Logger();
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;
        LOGGER_API ~Logger();

    public:
        template <typename... Args>
        void Info(Args... args)
        {
            Write(LogLevel::Info, Format(args...));
        }

        template <typename... Args>
        void Verbose(Args... args)
        {
            Write(LogLevel::Verbose, Format(args...));
        }

        template <typename... Args>
        void Warning(Args... args)
        {
            Write(LogLevel::Warning, Format(args...));
        }

        template <typename... Args>
        void Error(Args... args)
        {
            Write(LogLevel::Error, Format(args...));
        }

    private:
        template <typename... Args>
        static std::string Format(Args... args)
        {
            std::ostringstream out;
            ((out << args << ' '), ...);
            return out.str();
        }

        LOGGER_API void Write(LogLevel level, const std::string& message);

    private:
        std::unique_ptr<LogColorized> mColoriser;
        std::unique_ptr<LogFile>      mFile;
    };
}

template<class... Args>
void LOGINFO(Args... args)
{
    EngineLogger::Logger::Instance().Info(args...);
}
template<class... Args>
void LOGVERBOSE(Args... args)
{
    EngineLogger::Logger::Instance().Verbose(args...);
}
template<class... Args>
void LOGWARNING(Args... args)
{
    EngineLogger::Logger::Instance().Warning(args...);
}
template<class... Args>
void LOGERROR(Args... args)
{
    EngineLogger::Logger::Instance().Error(args...);
}
