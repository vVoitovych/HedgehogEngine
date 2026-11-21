#pragma once

#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"

#include <cstdint>

namespace HR
{
    // Where a mesh's indices sit in the registry's index buffer: LOD 0, the mesh itself, at
    // FirstIndex, and its levels of detail after it.
    struct MeshGeometryInfo
    {
        uint32_t                 FirstIndex   = 0;
        uint32_t                 IndexCount   = 0;
        uint32_t                 VertexOffset = 0;
        Renderer::MeshDrawRange  Draw;
    };
}
