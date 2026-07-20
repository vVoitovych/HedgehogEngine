#pragma once

#include "MeshGpuData.hpp"
#include "MaterialGpuData.hpp"

#include "HedgehogCommon/api/Resource/IResourceCatalog.hpp"
#include "RHI/api/RHITypes.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace FS
{
    class FileSystemManager;
}

namespace RHI
{
    class IRHIDevice;
    class IRHIBuffer;
    class IRHITexture;
    class IRHISampler;
    class IRHIDescriptorSetLayout;
    class IRHIDescriptorPool;
}

namespace HR
{
    class ResourceRegistry
    {
    public:
        explicit ResourceRegistry(RHI::IRHIDevice& device);
        ~ResourceRegistry();

        ResourceRegistry(const ResourceRegistry&)            = delete;
        ResourceRegistry& operator=(const ResourceRegistry&) = delete;
        ResourceRegistry(ResourceRegistry&&)                 = delete;
        ResourceRegistry& operator=(ResourceRegistry&&)      = delete;

        // Called once by the render pass that owns the material pipeline layout,
        // before any SyncMaterials call. maxSets must cover the full material budget.
        void SetMaterialLayout(RHI::IRHIDevice&                    device,
                               const RHI::IRHIDescriptorSetLayout& layout,
                               uint32_t                            maxSets,
                               const std::vector<RHI::PoolSize>&   poolSizes);

        void SyncMeshes(const HedgehogEngine::IResourceCatalog& catalog, RHI::IRHIDevice& device);
        void SyncMaterials(HedgehogEngine::IResourceCatalog& catalog, RHI::IRHIDevice& device);

        const RHI::IRHIBuffer& GetPositionsBuffer() const;
        const RHI::IRHIBuffer& GetTexCoordsBuffer() const;
        const RHI::IRHIBuffer& GetNormalsBuffer()   const;
        const RHI::IRHIBuffer& GetIndexBuffer()     const;

        const MeshGeometryInfo&       GetMeshGeometryInfo(size_t meshIndex) const;
        const RHI::IRHIDescriptorSet& GetMaterialDescriptorSet(uint32_t index) const;

        void Cleanup(RHI::IRHIDevice& device);

    private:
        RHI::IRHITexture& GetOrCreateTexture(const std::string& path, RHI::IRHIDevice& device,
                                              const FS::FileSystemManager& fileSystem);

        void CreateMaterialGpu(float transparency, const std::string& texturePath,
                                RHI::IRHIDevice& device, const FS::FileSystemManager& fileSystem);
        void UpdateMaterialGpu(uint32_t index, float transparency, const std::string& texturePath,
                               RHI::IRHIDevice& device, const FS::FileSystemManager& fileSystem);

        void FlushMeshUploads(RHI::IRHIDevice& device);

    private:
        struct MaterialUniform
        {
            float Transparency;
        };

    private:
        // Mesh data (CPU side)
        std::vector<float>    m_CpuPositions;
        std::vector<float>    m_CpuTexCoords;
        std::vector<float>    m_CpuNormals;
        std::vector<uint32_t> m_CpuIndices;
        bool                  m_MeshDataDirty       = false;
        size_t                m_RegisteredMeshCount = 0;

        // Per-mesh geometry info (index offset, count, vertex offset)
        std::vector<MeshGeometryInfo> m_MeshGeometryInfos;

        // Mesh GPU buffers
        std::unique_ptr<RHI::IRHIBuffer> m_PositionsBuffer;
        std::unique_ptr<RHI::IRHIBuffer> m_TexCoordsBuffer;
        std::unique_ptr<RHI::IRHIBuffer> m_NormalsBuffer;
        std::unique_ptr<RHI::IRHIBuffer> m_IndexBuffer;

        // Material GPU resources — layout is non-owning (owned by the render pass that defined it)
        const RHI::IRHIDescriptorSetLayout*      m_MaterialLayout = nullptr;
        std::unique_ptr<RHI::IRHIDescriptorPool> m_MaterialPool;
        std::vector<MaterialGpuData>             m_Materials;
        size_t                                   m_RegisteredMaterialCount = 0;

        // Shared texture cache and sampler
        std::unordered_map<std::string, std::unique_ptr<RHI::IRHITexture>> m_TextureCache;
        std::unique_ptr<RHI::IRHISampler>                                  m_LinearSampler;
    };
}
