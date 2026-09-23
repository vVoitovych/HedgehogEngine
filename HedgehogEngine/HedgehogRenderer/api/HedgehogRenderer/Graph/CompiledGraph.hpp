#pragma once

#include "RGTypes.hpp"

#include <cstddef>
#include <string>
#include <vector>

// What GraphCompiler::Compile produces from a GraphDescription (RENDERING.md section 5.3):
// culled, topologically sorted passes, each carrying the barriers that must run immediately
// before it, plus per-resource lifetime info for a future resource pool (C3). Passive data —
// GraphCompiler is what builds it.
namespace Renderer
{
    struct CompiledPass
    {
        // Index into the GraphDescription::Passes this was compiled from — a future execution
        // step uses this to find the pass's registered work (not part of C1/C2).
        size_t      OriginalPassIndex = 0;
        std::string Name;

        // Every barrier that must happen right before this pass runs, batched into the single
        // Barrier() call RENDERING.md section 5.3 step 5 describes — never one call per resource.
        std::vector<RGTextureBarrier> TextureBarriers;
        std::vector<RGBufferBarrier>  BufferBarriers;
    };

    struct ResourceLifetime
    {
        RGResourceId Id             = INVALID_RG_RESOURCE_ID;
        size_t       FirstUsePass   = 0; // index into CompiledGraph::Passes
        size_t       LastUsePass    = 0; // index into CompiledGraph::Passes
    };

    struct CompiledGraph
    {
        std::vector<CompiledPass>      Passes; // culled, sorted, execution order
        std::vector<ResourceLifetime>  ResourceLifetimes;
    };

    struct CompileError
    {
        std::string Message;      // full human-readable description, already names Pass/Resource
        std::string PassName;     // empty if not pass-specific (e.g. an unbound slot)
        std::string ResourceName; // empty if not resource-specific
    };
}
