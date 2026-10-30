#pragma once

#include <cstdint>
#include <string>

namespace Renderer
{
    // How long the GPU spent on one render-graph pass (its barriers and its commands), from GPU
    // timestamps; a pass several views run is summed over them.
    struct GpuPassTiming
    {
        std::string Name;
        double      GpuMilliseconds = 0.0;
    };

    // The GPU commands timed one by one while command timing is on (Renderer::SetCommandGpuTiming).
    enum class GpuCommandKind : uint8_t
    {
        Draw,         // Count vertices
        DrawIndexed,  // Count indices
        Barrier,      // Count texture and buffer transitions
        CopyBuffer,   // Count bytes
        CopyToTexture,
        Mipmaps,
        Blit,
    };

    [[nodiscard]] constexpr const char* GetGpuCommandKindName(GpuCommandKind kind)
    {
        switch (kind)
        {
        case GpuCommandKind::Draw:          return "Draw";
        case GpuCommandKind::DrawIndexed:   return "DrawIndexed";
        case GpuCommandKind::Barrier:       return "Barrier";
        case GpuCommandKind::CopyBuffer:    return "CopyBuffer";
        case GpuCommandKind::CopyToTexture: return "CopyToTexture";
        case GpuCommandKind::Mipmaps:       return "Mipmaps";
        case GpuCommandKind::Blit:          return "Blit";
        }
        return "?";
    }

    // One recorded GPU command: what it was, the pass that recorded it (a name that lives as long as
    // the process, Profiling::InternName) and, once the GPU has run it, how long from the previous
    // timed command's end to its own (so the binds and state changes before it count towards it).
    // The GPU overlaps consecutive draws, so one draw's time is approximate; a pass's is exact.
    struct GpuCommandTiming
    {
        const char*    Pass      = "";
        GpuCommandKind Kind      = GpuCommandKind::Draw;
        uint32_t       Count     = 0;
        uint32_t       Instances = 0;
        double         GpuMilliseconds = 0.0;
    };
}
