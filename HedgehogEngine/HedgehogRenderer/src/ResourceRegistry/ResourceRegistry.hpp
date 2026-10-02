#pragma once

#include "MeshGpuData.hpp"
#include "MaterialGpuData.hpp"

#include "HedgehogCommon/api/Resource/IResourceCatalog.hpp"
#include "RHI/api/RHITypes.hpp"

#include <cstdint>
#include <memory>
#include <span>
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

        // The game UI's textures: sets of the GameUi shader's set 0 (one combined image sampler),
        // MAX_UI_TEXTURE_SETS of them. Called once, before any SyncUiTextures call.
        static constexpr uint32_t MAX_UI_TEXTURE_SETS = 256;
        void SetUiTextureLayout(RHI::IRHIDevice& device, const RHI::IRHIDescriptorSetLayout& layout);

        // Loads every path under assets:// in paths not loaded yet (a missing file gets the magenta
        // placeholder, as a material's does) and gives it a UI texture set, and makes the white
        // solid-fill set on the first call. Paths past MAX_UI_TEXTURE_SETS get no set (one warning).
        void SyncUiTextures(std::span<const std::string> paths, RHI::IRHIDevice& device,
                            const FS::FileSystemManager& fileSystem);

        // The UI texture set of a synced path, or nullptr; and the white texture's set for a solid fill.
        const RHI::IRHIDescriptorSet* FindUiTextureSet(const std::string& path) const;
        const RHI::IRHIDescriptorSet* GetUiSolidTextureSet() const { return m_UiSolidSet.get(); }

        void SyncMeshes(const HedgehogEngine::IResourceCatalog& catalog, RHI::IRHIDevice& device);
        void SyncMaterials(HedgehogEngine::IResourceCatalog& catalog, RHI::IRHIDevice& device);

        const RHI::IRHIBuffer& GetPositionsBuffer() const;
        const RHI::IRHIBuffer& GetTexCoordsBuffer() const;
        const RHI::IRHIBuffer& GetNormalsBuffer()   const;
        const RHI::IRHIBuffer& GetIndexBuffer()     const;
        // Skinning streams, one entry per vertex like the position stream: joint indices (uint4)
        // and weights (float4), zero for static meshes (AppendSkinningStreams).
        const RHI::IRHIBuffer& GetJointsBuffer()    const;
        const RHI::IRHIBuffer& GetWeightsBuffer()   const;

        // The geometry buffers exist once at least one mesh has been synced.
        size_t GetMeshCount()     const { return m_MeshGeometryInfos.size(); }
        size_t GetMaterialCount() const { return m_Materials.size(); }

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
        std::vector<uint32_t> m_CpuJoints;
        std::vector<float>    m_CpuWeights;
        bool                  m_MeshDataDirty       = false;
        size_t                m_RegisteredMeshCount = 0;

        // Per-mesh geometry info (index offset, count, vertex offset)
        std::vector<MeshGeometryInfo> m_MeshGeometryInfos;

        // Mesh GPU buffers
        std::unique_ptr<RHI::IRHIBuffer> m_PositionsBuffer;
        std::unique_ptr<RHI::IRHIBuffer> m_TexCoordsBuffer;
        std::unique_ptr<RHI::IRHIBuffer> m_NormalsBuffer;
        std::unique_ptr<RHI::IRHIBuffer> m_IndexBuffer;
        std::unique_ptr<RHI::IRHIBuffer> m_JointsBuffer;
        std::unique_ptr<RHI::IRHIBuffer> m_WeightsBuffer;

        // Material GPU resources — layout is non-owning (owned by the render pass that defined it)
        const RHI::IRHIDescriptorSetLayout*      m_MaterialLayout = nullptr;
        std::unique_ptr<RHI::IRHIDescriptorPool> m_MaterialPool;
        std::vector<MaterialGpuData>             m_Materials;
        size_t                                   m_RegisteredMaterialCount = 0;

        // Shared texture cache and sampler
        std::unordered_map<std::string, std::unique_ptr<RHI::IRHITexture>> m_TextureCache;
        std::unique_ptr<RHI::IRHISampler>                                  m_LinearSampler;

        // Game UI textures: sampled clamped to their edges, one set each, and the white pixel.
        const RHI::IRHIDescriptorSetLayout*                                       m_UiTextureLayout = nullptr;
        std::unique_ptr<RHI::IRHIDescriptorPool>                                  m_UiTexturePool;
        std::unique_ptr<RHI::IRHISampler>                                         m_UiSampler;
        std::unordered_map<std::string, std::unique_ptr<RHI::IRHIDescriptorSet>> m_UiTextureSets;
        std::unique_ptr<RHI::IRHITexture>                                         m_UiSolidTexture;
        std::unique_ptr<RHI::IRHIDescriptorSet>                                   m_UiSolidSet;
        bool                                                                      m_WarnedUiTextureLimit = false;
    };
}
