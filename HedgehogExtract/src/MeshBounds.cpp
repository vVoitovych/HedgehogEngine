#include "api/MeshBounds.hpp"

#include "HedgehogCommon/api/Resource/IResourceCatalog.hpp"

namespace HX
{
    HM::AABB ComputeLocalBounds(std::span<const HM::Vector3> positions)
    {
        if (positions.empty())
            return HM::AABB(HM::Vector3(0.0f, 0.0f, 0.0f), HM::Vector3(0.0f, 0.0f, 0.0f));

        HM::AABB bounds(positions[0], positions[0]);
        for (const HM::Vector3& position : positions.subspan(1))
            bounds.ExpandToInclude(position);
        return bounds;
    }

    void MeshBoundsCache::Update(const HedgehogEngine::IResourceCatalog& catalog)
    {
        const size_t meshCount = catalog.GetMeshCount();
        if (meshCount < m_Bounds.size())
        {
            m_Bounds.clear(); // the catalog was rebuilt: recompute everything
            m_Versions.clear();
        }

        for (size_t i = 0; i < m_Bounds.size(); ++i)
        {
            const uint32_t version = catalog.GetMeshVersion(i);
            if (version != m_Versions[i])
            {
                m_Bounds[i]   = ComputeLocalBounds(catalog.GetMesh(i).positions);
                m_Versions[i] = version;
            }
        }
        for (size_t i = m_Bounds.size(); i < meshCount; ++i)
        {
            m_Bounds.push_back(ComputeLocalBounds(catalog.GetMesh(i).positions));
            m_Versions.push_back(catalog.GetMeshVersion(i));
        }
    }
}
