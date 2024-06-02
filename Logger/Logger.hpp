#pragma once

#define NOMINMAX

#include <iostream>
#include <ctime>
#include "LogColorized.hpp"

namespace EngineLogger
{
    class Logger 
    {
    public:
        static Logger& Instance() 
        {
            static Logger instance;
            return instance;
        }

    private:
        Logger()
        {

        }
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;
        ~Logger() = default;

    public:
        template <typename... Args>
        void Info(Args... args) 
        {
            mColoriser.SetLogColor(7);
            std::cout << "[INFO]";
            LogHelper(args...);
            std::cout << std::endl;
        }
        template <typename... Args>
        void Verbose(Args... args)
        {
            mColoriser.SetLogColor(10);
            std::cout << "[VERBOSE]";
            LogHelper(args...);
            std::cout << std::endl;
        }
        template <typename... Args>
        void Warning(Args... args) 
        {
            mColoriser.SetLogColor(14);
            std::cout << "[WARNING]";
            LogHelper(args...);
            std::cout << std::endl;
        }

        template <typename... Args>
        void Error(Args... args) 
        {
            mColoriser.SetLogColor(12);
            std::cout << "[ERROR]";
            LogHelper(args...);
            std::cout << std::endl;
        }
    private:

        template <typename T, typename... Args>
        void LogHelper(T first, Args... args) 
        {
            std::cout << first << ' ';
            LogHelper(args...);
        }

        void LogHelper()
        {
        }
    private:
        LogColorized mColoriser;
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
void  LOGWARNING(Args... args)
{
    EngineLogger::Logger::Instance().Warning(args...);
}
template<class... Args>
void  LOGERROR(Args... args)
{
    EngineLogger::Logger::Instance().Error(args...);
}






