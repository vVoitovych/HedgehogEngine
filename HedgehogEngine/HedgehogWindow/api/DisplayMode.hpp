#pragma once

#include "HedgehogEngine/HedgehogWindow/api/HedgehogWindowApi.hpp"

#include <optional>
#include <span>
#include <vector>

namespace HW
{
    enum class WindowMode
    {
        Windowed,
        Borderless, // covers the monitor at its current mode, no display mode switch
        Fullscreen  // exclusive, at a video mode of the monitor
    };

    struct VideoMode
    {
        int Width       = 0;
        int Height      = 0;
        int RefreshRate = 0;

        bool operator==(const VideoMode&) const = default;
    };

    // One entry per size (the highest refresh rate kept), sorted by area then width, smallest first;
    // modes with a non-positive size are dropped.
    [[nodiscard]] HEDGEHOG_WINDOW_API std::vector<VideoMode> NormalizeVideoModes(std::vector<VideoMode> modes);

    // Picks from a normalized list: the exact size; else the first mode at least as wide and as tall;
    // else (asked for more than any mode covers, or for nothing: a size that is not positive) the
    // largest. Nothing for an empty list.
    [[nodiscard]] HEDGEHOG_WINDOW_API std::optional<VideoMode> ChooseVideoMode(std::span<const VideoMode> modes,
                                                                               int width, int height);
}
