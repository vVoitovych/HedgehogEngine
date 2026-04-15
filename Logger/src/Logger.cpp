#include "api/Logger.hpp"
#include "LogColorized.hpp"

namespace EngineLogger
{
    Logger::Logger()
        : mColoriser(std::make_unique<LogColorized>())
    {
    }

    Logger::~Logger() = default;

    Logger& Logger::Instance()
    {
        static Logger instance;
        return instance;
    }

    void Logger::SetColor(uint16_t color)
    {
        mColoriser->SetLogColor(color);
    }
}
