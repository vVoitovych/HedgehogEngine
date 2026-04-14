#include "LogColorized.hpp"

#ifdef _WIN32
#include <Windows.h>
#else
#include <iostream>
#endif

namespace EngineLogger
{
    LogColorized::LogColorized()
    {
#ifdef _WIN32
        mConsole = GetStdHandle(STD_OUTPUT_HANDLE);
#endif
    }

    void LogColorized::SetLogColor(uint16_t color)
    {
#ifdef _WIN32
        SetConsoleTextAttribute(mConsole, color);
#else
        // Map Windows console color codes to ANSI escape sequences
        switch (color)
        {
        case 10: std::cout << "\033[92m"; break; // bright green  (VERBOSE)
        case 14: std::cout << "\033[93m"; break; // bright yellow (WARNING)
        case 12: std::cout << "\033[91m"; break; // bright red    (ERROR)
        default: std::cout << "\033[0m";  break; // reset         (INFO / reset)
        }
#endif
    }
}

