#pragma once

#include "HedgehogCommon/api/Resource/IResourceCatalog.hpp"

#include <vector>

namespace HR
{
    // Appends one mesh's tangents to the shared stream, four floats per vertex of mesh.positions so
    // it stays aligned with the position stream: xyz along increasing u, w the handedness. A mesh
    // whose tangents do not cover every vertex appends DEFAULT_TANGENT for each.
    void AppendTangentStream(const HedgehogEngine::MeshView& mesh, std::vector<float>& tangents);

    // ContentLoader's tangent for a vertex it has none for: +X, right-handed.
    inline constexpr float DEFAULT_TANGENT[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
}
