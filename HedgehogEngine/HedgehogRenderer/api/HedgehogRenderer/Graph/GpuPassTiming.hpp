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
        ScopeBegin,   // a named scope opens (IRHICommandList::BeginScope); Scope names it
        ScopeEnd,     // the innermost open scope closes
    };

    // A scope marker rather than work the GPU does.
    [[nodiscard]] constexpr bool IsGpuScopeMarker(GpuCommandKind kind)
    {
        return kind == GpuCommandKind::ScopeBegin || kind == GpuCommandKind::ScopeEnd;
    }

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
        case GpuCommandKind::ScopeBegin:    return "ScopeBegin";
        case GpuCommandKind::ScopeEnd:      return "ScopeEnd";
        }
        return "?";
    }

    // One recorded GPU command: what it was, the pass that recorded it (a name that lives as long as
    // the process, Profiling::InternName) and, once the GPU has run it, how long from the previous
    // timed command's end to its own (so the binds and state changes before it count towards it).
    // The GPU overlaps consecutive draws, so one draw's time is approximate; a pass's is exact.
    // Scope markers are timed too: a ScopeBegin's timestamp is where its scope starts and the
    // matching ScopeEnd's where it ends.
    struct GpuCommandTiming
    {
        const char*    Pass      = "";
        GpuCommandKind Kind      = GpuCommandKind::Draw;
        uint32_t       Count     = 0;
        uint32_t       Instances = 0;
        double         GpuMilliseconds = 0.0;
        const char*    Scope     = nullptr; // a ScopeBegin's name
    };
}
