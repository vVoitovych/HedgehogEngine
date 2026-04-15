#pragma once

#ifdef _WIN32
#define NOMINMAX
#endif

#include <iostream>
#include <cstdint>
#include "LoggerApi.hpp"

namespace EngineLogger
{
    class LogColorized;

    class LOGGER_API Logger
    {
    public:
        static Logger& Instance();

    private:
        Logger();
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;
        ~Logger();

    public:
        template <typename... Args>
        void Info(Args... args)
        {
            SetColor(7);
            std::cout << "[INFO]";
            LogHelper(args...);
            std::cout << std::endl;
            SetColor(7);
        }

        template <typename... Args>
        void Verbose(Args... args)
        {
            SetColor(10);
            std::cout << "[VERBOSE]";
            LogHelper(args...);
            std::cout << std::endl;
            SetColor(7);
        }

        template <typename... Args>
        void Warning(Args... args)
        {
            SetColor(14);
            std::cout << "[WARNING]";
            LogHelper(args...);
            std::cout << std::endl;
            SetColor(7);
        }

        template <typename... Args>
        void Error(Args... args)
        {
            SetColor(12);
            std::cout << "[ERROR]";
            LogHelper(args...);
            std::cout << std::endl;
            SetColor(7);
        }

    private:
        template <typename T, typename... Args>
        void LogHelper(T first, Args... args)
        {
            std::cout << first << ' ';
            LogHelper(args...);
        }

        void LogHelper() {}

        void SetColor(uint16_t color);

    private:
        LogColorized* mColoriser;
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
