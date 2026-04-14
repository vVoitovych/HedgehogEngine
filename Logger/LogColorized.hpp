#pragma once
#include <stdint.h>
#include "LoggerApi.hpp"

namespace EngineLogger
{
    class LOGGER_API LogColorized
    {
    public:
        LogColorized();

        void SetLogColor(uint16_t color);
    private:
#ifdef _WIN32
        void* mConsole;
#endif
    };
}





