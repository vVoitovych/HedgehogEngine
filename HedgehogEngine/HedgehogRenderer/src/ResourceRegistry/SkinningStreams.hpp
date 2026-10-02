#pragma once

#include "HedgehogCommon/api/Resource/IResourceCatalog.hpp"

#include <cstdint>
#include <vector>

namespace HR
{
    // Appends one mesh's skinning vertex streams to the shared ones, four values per vertex of
    // mesh.positions so they stay aligned with the position stream: joint indices as uint4 and
    // weights as float4. A static mesh, or one whose joints or weights do not cover every vertex,
    // appends zeros (no joint, no weight).
    void AppendSkinningStreams(const HedgehogEngine::MeshView& mesh, std::vector<uint32_t>& joints,
                               std::vector<float>& weights);
}
