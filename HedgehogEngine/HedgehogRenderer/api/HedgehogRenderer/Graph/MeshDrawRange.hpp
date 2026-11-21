#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

namespace Renderer
{
    // The mesh itself and up to three levels of detail.
    inline constexpr uint32_t MAX_MESH_LODS = 4;

    // One level of detail's indices in the shared index buffer.
    struct MeshLodRange
    {
        uint32_t FirstIndex = 0;
        uint32_t IndexCount = 0;
    };

    // Where one mesh's indices sit in the shared geometry buffers: FirstIndex and IndexCount are LOD 0,
    // the mesh itself (LodRanges[0]), and LodRanges[1 .. LodCount - 1] its levels of detail, all over
    // the same vertices at VertexOffset.
    struct MeshDrawRange
    {
        uint32_t                                  FirstIndex   = 0;
        uint32_t                                  IndexCount   = 0;
        uint32_t                                  VertexOffset = 0;
        std::array<MeshLodRange, MAX_MESH_LODS>   LodRanges{};
        uint32_t                                  LodCount     = 1;
    };

    // A level of detail's range, clamped to the mesh's coarsest level (LOD 0 for a mesh without).
    [[nodiscard]] inline MeshLodRange GetLodRange(const MeshDrawRange& mesh, uint32_t lod)
    {
        if (lod == 0 || mesh.LodCount <= 1)
            return { mesh.FirstIndex, mesh.IndexCount };
        return mesh.LodRanges[std::min(lod, std::min(mesh.LodCount, MAX_MESH_LODS) - 1)];
    }
}
