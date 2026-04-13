#include "Logger.hpp"

namespace EngineLogger
{
    Logger::Logger()
    {
    }

    Logger& Logger::Instance()
    {
        static Logger instance;
        return instance;
    }
}
