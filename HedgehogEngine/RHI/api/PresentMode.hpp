#pragma once

#include <algorithm>
#include <cstdint>
#include <span>

namespace RHI
{

enum class PresentMode : uint8_t
{
    VSync,
    NoVSync
};

// The surface modes the engine chooses between; a surface's other modes are ignored.
enum class SurfacePresentMode : uint8_t
{
    Fifo,
    Mailbox,
    Immediate
};

// VSync: Fifo (always supported). NoVSync: Mailbox, else Immediate, else Fifo.
[[nodiscard]] inline constexpr SurfacePresentMode ChoosePresentMode(
    std::span<const SurfacePresentMode> available, PresentMode mode) noexcept
{
    if (mode == PresentMode::VSync)
    {
        return SurfacePresentMode::Fifo;
    }
    if (std::ranges::find(available, SurfacePresentMode::Mailbox) != available.end())
    {
        return SurfacePresentMode::Mailbox;
    }
    if (std::ranges::find(available, SurfacePresentMode::Immediate) != available.end())
    {
        return SurfacePresentMode::Immediate;
    }
    return SurfacePresentMode::Fifo;
}

} // namespace RHI
