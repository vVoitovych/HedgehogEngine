#include "HedgehogEngine/HedgehogWindow/api/DisplayMode.hpp"

#include <algorithm>
#include <cstdint>
#include <tuple>

namespace HW
{
    namespace
    {
        int64_t Area(const VideoMode& mode)
        {
            return static_cast<int64_t>(mode.Width) * mode.Height;
        }
    }

    std::vector<VideoMode> NormalizeVideoModes(std::vector<VideoMode> modes)
    {
        std::erase_if(modes, [](const VideoMode& mode) { return mode.Width <= 0 || mode.Height <= 0; });
        std::ranges::sort(modes, [](const VideoMode& a, const VideoMode& b)
        {
            return std::make_tuple(Area(a), a.Width, a.Height, -a.RefreshRate) <
                   std::make_tuple(Area(b), b.Width, b.Height, -b.RefreshRate);
        });
        const auto duplicates = std::ranges::unique(modes, [](const VideoMode& a, const VideoMode& b)
        {
            return a.Width == b.Width && a.Height == b.Height;
        });
        modes.erase(duplicates.begin(), duplicates.end());
        return modes;
    }

    std::optional<VideoMode> ChooseVideoMode(std::span<const VideoMode> modes, int width, int height)
    {
        if (modes.empty())
        {
            return std::nullopt;
        }
        if (width <= 0 || height <= 0)
        {
            return modes.back();
        }
        for (const VideoMode& mode : modes)
        {
            if (mode.Width == width && mode.Height == height)
            {
                return mode;
            }
        }
        for (const VideoMode& mode : modes)
        {
            if (mode.Width >= width && mode.Height >= height)
            {
                return mode;
            }
        }
        return modes.back();
    }
}
