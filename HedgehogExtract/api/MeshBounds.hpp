#pragma once

#include "HedgehogMath/api/AABB.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include <span>
#include <vector>

namespace HedgehogEngine
{
    class IResourceCatalog;
}

namespace HX
{
    // The smallest box around positions; a degenerate box at the origin when there are none.
    [[nodiscard]] HM::AABB ComputeLocalBounds(std::span<const HM::Vector3> positions);

    // Local-space bounds per mesh, indexed like the resource catalog's meshes, for
    // SceneExtractor::Extract. Meshes are only ever appended to the catalog, so Update computes the
    // bounds of new meshes only: a steady-state frame touches no vertex data.
    class MeshBoundsCache
    {
    public:
        void Update(const HedgehogEngine::IResourceCatalog& catalog);

        [[nodiscard]] std::span<const HM::AABB> GetBounds() const { return m_Bounds; }

    private:
        std::vector<HM::AABB> m_Bounds;
    };
}
