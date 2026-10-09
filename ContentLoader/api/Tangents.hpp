#pragma once

#include "ContentLoaderApi.hpp"
#include "LoadedData.hpp"

#include <cstdint>
#include <vector>

namespace ContentLoader
{
    // Gives every vertex a tangent for normal mapping: xyz a unit vector orthogonal to its normal
    // along increasing u, w the handedness (+1 or -1) such that the bitangent, along increasing v, is
    // cross(normal, tangent.xyz) * w, as glTF defines TANGENT. Each triangle's UV derivatives are
    // summed into its three vertices (weighted by the triangle's size), then each sum is made
    // orthogonal to the normal (Gram-Schmidt). MikkTSpace-like, not identical to it. A vertex whose
    // triangles have no usable UVs (all the same, or along a line) gets any unit vector orthogonal to
    // its normal with w +1; one with no normal gets (1, 0, 0, 1). indices are triangles; a trailing
    // index short of a triangle, or one out of range, is ignored.
    CONTENT_LOADER_API void GenerateTangents(std::vector<LoadedVertexData>& vertices,
                                             const std::vector<uint32_t>&   indices);
}
