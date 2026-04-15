#include "api/Logger.hpp"
#include "LogColorized.hpp"

namespace EngineLogger
{
    Logger::Logger()
        : mColoriser(new LogColorized())
    {
    }

    Logger::~Logger()
    {
        delete mColoriser;
        mColoriser = nullptr;
    }

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
