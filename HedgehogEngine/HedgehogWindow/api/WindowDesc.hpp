#pragma once

#include <string>

namespace HW
{
    struct WindowDesc
    {
        std::string m_Title      = "HedgehogEngine";
        int         m_X          = 100;
        int         m_Y          = 100;
        int         m_Width      = 1366;
        int         m_Height     = 768;
        bool        m_Fullscreen = false;
    };
}
