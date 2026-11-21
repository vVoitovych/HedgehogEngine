#include "api/MeshLods.hpp"

#include "meshoptimizer.h"

#include <algorithm>

namespace ContentLoader
{
    std::vector<std::vector<uint32_t>> GenerateMeshLods(std::span<const HM::Vector3> positions,
                                                        std::span<const uint32_t>    indices,
                                                        const MeshLodDesc&           desc)
    {
        std::vector<std::vector<uint32_t>> levels;
        if (indices.size() % 3 != 0 || indices.size() / 3 < MIN_LOD_TRIANGLES)
            return levels;
        if (std::any_of(indices.begin(), indices.end(), [&](uint32_t index) { return index >= positions.size(); }))
            return levels;

        // Tightly packed floats, whatever HM::Vector3's layout.
        std::vector<float> packed;
        packed.reserve(positions.size() * 3);
        for (const HM::Vector3& p : positions)
            packed.insert(packed.end(), { p.x(), p.y(), p.z() });

        std::span<const uint32_t> previous = indices;
        for (uint32_t level = 0; level < desc.MaxLevels; ++level)
        {
            const size_t target = std::max<size_t>(previous.size() / 2 / 3 * 3, 3);
            std::vector<uint32_t> simplified(previous.size());
            // Each level from the one before, so they nest and the coarse ones cost less to make.
            const size_t count = meshopt_simplify(simplified.data(), previous.data(), previous.size(), packed.data(),
                                                  positions.size(), sizeof(float) * 3, target, desc.MaxError, 0, nullptr);
            if (count == 0 || static_cast<float>(count) > static_cast<float>(previous.size()) * desc.MinShrink)
                break;
            simplified.resize(count);
            levels.push_back(std::move(simplified));
            previous = levels.back();
        }
        return levels;
    }
}
