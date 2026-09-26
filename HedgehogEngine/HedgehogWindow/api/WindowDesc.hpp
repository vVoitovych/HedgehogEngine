#pragma once

#include <string>

namespace HW
{
    struct WindowDesc
    {
        std::string Title      = "HedgehogEngine";
        int         X          = 100;
        int         Y          = 100;
        int         Width      = 1366;
        int         Height     = 768;
        bool        Fullscreen = false;
        // Opens maximized on its monitor. X, Y, Width and Height remain the restored geometry.
        // Ignored when Fullscreen is set.
        bool        Maximized  = false;
    };
}
