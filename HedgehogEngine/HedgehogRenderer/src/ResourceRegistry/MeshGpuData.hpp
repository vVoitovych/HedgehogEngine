#pragma once

#include <cstdint>

namespace HR
{
    struct MeshGeometryInfo
    {
        uint32_t m_FirstIndex   = 0;
        uint32_t m_IndexCount   = 0;
        uint32_t m_VertexOffset = 0;
    };
}
