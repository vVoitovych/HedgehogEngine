#include "HedgehogEngine/HedgehogWindow/api/TitleBar.hpp"

namespace HW
{
    TitleBarHit HitTestTitleBar(int x, int y, bool maximized, int topBorder, std::span<const ClientRect> captionAreas)
    {
        if (!maximized && y >= 0 && y < topBorder)
            return TitleBarHit::Top;
        for (const ClientRect& area : captionAreas)
        {
            if (x >= area.X && x < area.X + area.Width && y >= area.Y && y < area.Y + area.Height)
                return TitleBarHit::Caption;
        }
        return TitleBarHit::Client;
    }
}
