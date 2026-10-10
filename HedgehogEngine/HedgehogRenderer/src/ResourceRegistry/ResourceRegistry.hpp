#pragma once

#include "MeshGpuData.hpp"
#include "MaterialGpuData.hpp"
#include "MaterialUniform.hpp"

#include "HedgehogRenderer/Views/ViewCulling.hpp"

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

        // Uploads every font atlas the catalog baked since the last call (fonts are only appended) as a
        // white texture with the glyphs' coverage in alpha, each with a UI texture set. Fonts past
        // MAX_UI_FONT_SETS get no set (one warning), so their text is not drawn.
        static constexpr uint32_t MAX_UI_FONT_SETS = 64;
        void SyncFonts(const HedgehogEngine::IResourceCatalog& catalog, RHI::IRHIDevice& device);

        // The UI texture set of a synced path (any spelling SyncUiTextures was given), or nullptr; and the
        // white texture's set for a solid fill. A lookup of the spelling, allocating nothing.
        const RHI::IRHIDescriptorSet* FindUiTextureSet(const std::string& path) const;
        // The set of the catalog's font index, or nullptr when it has none.
        const RHI::IRHIDescriptorSet* FindUiFontSet(size_t font) const;
        const RHI::IRHIDescriptorSet* GetUiSolidTextureSet() const { return m_UiSolidSet.get(); }

        void SyncMeshes(const HedgehogEngine::IResourceCatalog& catalog, RHI::IRHIDevice& device);
        void SyncMaterials(HedgehogEngine::IResourceCatalog& catalog, RHI::IRHIDevice& device);

        const RHI::IRHIBuffer& GetPositionsBuffer() const;
        const RHI::IRHIBuffer& GetTexCoordsBuffer() const;
        const RHI::IRHIBuffer& GetNormalsBuffer()   const;
        // One tangent (float4: xyz, handedness) per vertex, aligned with the positions (AppendTangentStream).
        const RHI::IRHIBuffer& GetTangentsBuffer()  const;
        const RHI::IRHIBuffer& GetIndexBuffer()     const;
        // Skinning streams, one entry per vertex like the position stream: joint indices (uint4)
        // and weights (float4), zero for static meshes (AppendSkinningStreams).
        const RHI::IRHIBuffer& GetJointsBuffer()    const;
        const RHI::IRHIBuffer& GetWeightsBuffer()   const;

        // The geometry buffers exist once at least one mesh has been synced.
        size_t GetMeshCount()     const { return m_MeshGeometryInfos.size(); }
        size_t GetMaterialCount() const { return m_Materials.size(); }
        // Each material's alpha mode and sidedness, by material index (GraphFrameData::Materials).
        std::span<const Renderer::MaterialDrawInfo> GetMaterialDrawInfos() const { return m_MaterialDrawInfos; }

        const MeshGeometryInfo&       GetMeshGeometryInfo(size_t meshIndex) const;
        const RHI::IRHIDescriptorSet& GetMaterialDescriptorSet(uint32_t index) const;

        void Cleanup(RHI::IRHIDevice& device);

    private:
        // The texture of a path, loaded once per colour space: srgb for colour (a base colour or
        // emissive map, a UI texture), else linear for data (normal, metallic-roughness, occlusion).
        RHI::IRHITexture& GetOrCreateTexture(const std::string& path, RHI::IRHIDevice& device,
                                              const FS::FileSystemManager& fileSystem, bool srgb = true);

        void CreateMaterialGpu(const HedgehogEngine::MaterialView& material, RHI::IRHIDevice& device,
                               const FS::FileSystemManager& fileSystem);
        void UpdateMaterialGpu(uint32_t index, const HedgehogEngine::MaterialView& material, RHI::IRHIDevice& device,
                               const FS::FileSystemManager& fileSystem);
        // Writes the uniform and every slot's texture (its map, else its neutral default) into set.
        void WriteMaterialSet(MaterialGpuData& gpu, const HedgehogEngine::MaterialView& material, RHI::IRHIDevice& device,
                              const FS::FileSystemManager& fileSystem);

        void FlushMeshUploads(RHI::IRHIDevice& device);

    private:
        // Mesh data (CPU side)
        std::vector<float>    m_CpuPositions;
        std::vector<float>    m_CpuTexCoords;
        std::vector<float>    m_CpuNormals;
        std::vector<float>    m_CpuTangents;
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
        std::unique_ptr<RHI::IRHIBuffer> m_TangentsBuffer;
        std::unique_ptr<RHI::IRHIBuffer> m_IndexBuffer;
        std::unique_ptr<RHI::IRHIBuffer> m_JointsBuffer;
        std::unique_ptr<RHI::IRHIBuffer> m_WeightsBuffer;

        // Material GPU resources — layout is non-owning (owned by the render pass that defined it)
        const RHI::IRHIDescriptorSetLayout*      m_MaterialLayout = nullptr;
        std::unique_ptr<RHI::IRHIDescriptorPool> m_MaterialPool;
        std::vector<MaterialGpuData>             m_Materials;
        std::vector<Renderer::MaterialDrawInfo>  m_MaterialDrawInfos; // aligned with m_Materials
        size_t                                   m_RegisteredMaterialCount = 0;

        // Shared texture cache (keyed by path, with a suffix for a linear copy) and sampler
        std::unordered_map<std::string, std::unique_ptr<RHI::IRHITexture>> m_TextureCache;
        std::unique_ptr<RHI::IRHISampler>                                  m_LinearSampler;

        // A material slot without a map samples these 1x1 textures, which leave its factors as they
        // are: white (sRGB for colour, linear for data) and the flat normal (0.5, 0.5, 1).
        std::unique_ptr<RHI::IRHITexture> m_WhiteSrgbTexture;
        std::unique_ptr<RHI::IRHITexture> m_WhiteLinearTexture;
        std::unique_ptr<RHI::IRHITexture> m_FlatNormalTexture;

        // Game UI textures: sampled clamped to their edges, one set per file (by FS::MakeAssetKey, so
        // every spelling of one image shares it and counts once against MAX_UI_TEXTURE_SETS), each
        // spelling seen pointing at its set, and the white pixel.
        const RHI::IRHIDescriptorSetLayout*                                       m_UiTextureLayout = nullptr;
        std::unique_ptr<RHI::IRHIDescriptorPool>                                  m_UiTexturePool;
        std::unique_ptr<RHI::IRHISampler>                                         m_UiSampler;
        std::unordered_map<std::string, std::unique_ptr<RHI::IRHIDescriptorSet>> m_UiTextureSets; // by key
        std::unordered_map<std::string, const RHI::IRHIDescriptorSet*>            m_UiTextureSpellings;
        std::unique_ptr<RHI::IRHITexture>                                         m_UiSolidTexture;
        std::unique_ptr<RHI::IRHIDescriptorSet>                                   m_UiSolidSet;
        bool                                                                      m_WarnedUiTextureLimit = false;

        // Font atlases, by the catalog's font index; past MAX_UI_FONT_SETS the set is null.
        std::vector<std::unique_ptr<RHI::IRHITexture>>       m_FontTextures;
        std::vector<std::unique_ptr<RHI::IRHIDescriptorSet>> m_FontSets;
    };
}
