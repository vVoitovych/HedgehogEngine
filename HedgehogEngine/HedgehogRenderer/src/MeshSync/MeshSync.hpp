#pragma once

#include <cstdint>
#include <vector>

namespace HedgehogEngine
{
    class MeshContainer;
}

namespace RHI
{
    class IRHIDevice;
}

namespace Renderer
{
    class ResourceManager;

    struct MeshGeometryInfo
    {
        uint32_t m_FirstIndex   = 0;
        uint32_t m_IndexCount   = 0;
        uint32_t m_VertexOffset = 0;
    };

    class MeshSync
    {
    public:
        MeshSync()  = default;
        ~MeshSync() = default;

        MeshSync(const MeshSync&)            = delete;
        MeshSync& operator=(const MeshSync&) = delete;
        MeshSync(MeshSync&&)                 = delete;
        MeshSync& operator=(MeshSync&&)      = delete;

        void Sync(const HedgehogEngine::MeshContainer& container,
                  ResourceManager&                     resourceManager,
                  RHI::IRHIDevice&                     device);

        const MeshGeometryInfo& GetMeshGeometryInfo(size_t meshIndex) const;

    private:
        void FlushMeshUploads(ResourceManager& resourceManager, RHI::IRHIDevice& device);

    private:
        std::vector<float>    m_CpuPositions;
        std::vector<float>    m_CpuTexCoords;
        std::vector<float>    m_CpuNormals;
        std::vector<uint32_t> m_CpuIndices;
        bool                  m_MeshDataDirty       = false;
        size_t                m_RegisteredMeshCount = 0;

        std::vector<MeshGeometryInfo> m_MeshGeometryInfos;
    };
}
