#pragma once

#include "RGTypes.hpp"

#include <string>
#include <vector>

// The device-free record a GraphBuilder accumulates (RENDERING.md section 5.1: "a device-free
// graph description recording everything declared"). Passive data throughout — GraphBuilder
// and RGPassBuilder are what mutate it; nothing here validates or compiles anything. That is
// GraphCompiler's job (a later ticket, RENDERING.md section 5.3), not this one.
namespace Renderer
{
    // One logical resource: everything the graph knows about it regardless of how many
    // versions it has gone through. IsBuffer selects which of TextureDesc/BufferDesc is valid.
    struct RGResourceRecord
    {
        RGResourceId   Id           = INVALID_RG_RESOURCE_ID;
        std::string    Name;
        bool           IsImported   = false;
        bool           IsReadOnly   = false; // imported resources only; Compile rejects a write to one
        bool           IsBuffer     = false;
        RGTextureDesc  TextureDesc;
        RGBufferDesc   BufferDesc;
        RGVersion      LatestVersion = 0;
    };

    // One read or write a pass recorded against a resource. For a write, Version is the NEW
    // version the write produced (never a version seen before); for a read, the version being
    // read. A read-after-write or write-after-read edge between two passes is recoverable by
    // finding another pass whose Reads/Writes name the same (Id, Version) pair — this ticket
    // records the information the edge is derived from; deriving the DAG itself is Compile's job.
    struct RGResourceRef
    {
        RGResourceId    Id      = INVALID_RG_RESOURCE_ID;
        RGVersion       Version = 0;
        RGResourceUsage Usage   = RGResourceUsage::SampledTexture;
    };

    struct RGPassRecord
    {
        std::string                Name;
        std::vector<RGResourceRef> Reads;
        std::vector<RGResourceRef> Writes;
        bool                       HasSideEffect = false;
    };

    struct GraphDescription
    {
        std::vector<RGResourceRecord> Resources;
        std::vector<RGPassRecord>     Passes;
        std::vector<RGOutputSlot>     OutputSlots;

        const RGResourceRecord* FindResource(RGResourceId id) const
        {
            for (const auto& resource : Resources)
            {
                if (resource.Id == id)
                    return &resource;
            }
            return nullptr;
        }
    };
}
