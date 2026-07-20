#pragma once

#include <cstdint>

namespace HR
{
    struct MeshGeometryInfo
    {
        uint32_t FirstIndex   = 0;
        uint32_t IndexCount   = 0;
        uint32_t VertexOffset = 0;
    };
}
