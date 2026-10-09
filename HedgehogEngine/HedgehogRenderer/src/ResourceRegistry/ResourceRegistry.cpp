#include "ResourceRegistry.hpp"
#include "SkinningStreams.hpp"
#include "TangentStream.hpp"

#include "ContentLoader/api/TextureLoader.hpp"
#include "FileSystem/api/PathUtils.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHIBuffer.hpp"
#include "RHI/api/IRHITexture.hpp"
#include "RHI/api/IRHISampler.hpp"
#include "RHI/api/IRHIDescriptor.hpp"
#include "RHI/api/IRHICommandList.hpp"

#include "Logger/api/Logger.hpp"

#include <cassert>

namespace HR
{
    namespace
    {
        // A sampled RGBA8 texture holding width x height pixels, uploaded and ready to read: sRGB, or
        // linear (Unorm) for data; with mipmapped, every level of a full mip chain is generated from
        // them.
        std::unique_ptr<RHI::IRHITexture> UploadRgba8(RHI::IRHIDevice& device, const void* pixels, uint32_t width,
                                                      uint32_t height, bool mipmapped, bool srgb = true)
        {
            const size_t imgSize = static_cast<size_t>(width) * height * 4;
            auto staging = device.CreateBuffer(imgSize, RHI::BufferUsage::TransferSrc, RHI::MemoryUsage::CpuToGpu);
            staging->CopyData(pixels, imgSize);

            RHI::TextureDesc desc;
            desc.Width     = width;
            desc.Height    = height;
            desc.Format    = srgb ? RHI::Format::R8G8B8A8Srgb : RHI::Format::R8G8B8A8Unorm;
            desc.Usage     = RHI::TextureUsage::Sampled | RHI::TextureUsage::TransferDst;
            desc.MipLevels = mipmapped ? RHI::GetMipLevelCount(width, height) : 1;
            if (mipmapped)
                desc.Usage = desc.Usage | RHI::TextureUsage::TransferSrc;
            auto texture = device.CreateTexture(desc);

            device.ExecuteImmediately([&](RHI::IRHICommandList& cmd)
            {
                const RHI::TextureBarrier toCopy{ texture.get(), RHI::ResourceState::Undefined, RHI::ResourceState::CopyDst };
                cmd.Barrier({ &toCopy, 1 }, {});
                cmd.CopyBufferToTexture(*staging, *texture);
                if (mipmapped)
                {
                    cmd.GenerateMipmaps(*texture);
                    return;
                }
                const RHI::TextureBarrier toRead{ texture.get(), RHI::ResourceState::CopyDst, RHI::ResourceState::ShaderResource };
                cmd.Barrier({ &toRead, 1 }, {});
            });
            return texture;
        }
    }

    ResourceRegistry::ResourceRegistry(RHI::IRHIDevice& device)
    {
        RHI::SamplerDesc samplerDesc;
        samplerDesc.MinFilter    = RHI::Filter::Linear;
        samplerDesc.MagFilter    = RHI::Filter::Linear;
        samplerDesc.AddressModeU = RHI::AddressMode::Repeat;
        samplerDesc.AddressModeV = RHI::AddressMode::Repeat;
        samplerDesc.AddressModeW = RHI::AddressMode::Repeat;
        m_LinearSampler = device.CreateSampler(samplerDesc);
    }

    ResourceRegistry::~ResourceRegistry()
    {
    }

    void ResourceRegistry::SetMaterialLayout(RHI::IRHIDevice&                    device,
                                              const RHI::IRHIDescriptorSetLayout& layout,
                                              uint32_t                            maxSets,
                                              const std::vector<RHI::PoolSize>&   poolSizes)
    {
        assert(m_Materials.empty() && "SetMaterialLayout must be called before any materials are registered");
        m_MaterialLayout = &layout;
        m_MaterialPool   = device.CreateDescriptorPool(maxSets, poolSizes);
    }

    void ResourceRegistry::SyncMeshes(const HedgehogEngine::IResourceCatalog& catalog, RHI::IRHIDevice& device)
    {
        const size_t totalMeshes = catalog.GetMeshCount();
        if (totalMeshes <= m_RegisteredMeshCount)
            return;

        for (size_t i = m_RegisteredMeshCount; i < totalMeshes; ++i)
        {
            const HedgehogEngine::MeshView mesh = catalog.GetMesh(i);

            MeshGeometryInfo geom;
            geom.FirstIndex   = mesh.firstIndex;
            geom.IndexCount   = mesh.indexCount;
            geom.VertexOffset = mesh.vertexOffset;
            m_MeshGeometryInfos.push_back(geom);

            for (const auto& p : mesh.positions)
            {
                m_CpuPositions.push_back(p.x());
                m_CpuPositions.push_back(p.y());
                m_CpuPositions.push_back(p.z());
            }
            for (const auto& uv : mesh.texCoords)
            {
                m_CpuTexCoords.push_back(uv.x());
                m_CpuTexCoords.push_back(uv.y());
            }
            for (const auto& n : mesh.normals)
            {
                m_CpuNormals.push_back(n.x());
                m_CpuNormals.push_back(n.y());
                m_CpuNormals.push_back(n.z());
            }
            for (uint32_t idx : mesh.indices)
                m_CpuIndices.push_back(idx);
            AppendTangentStream(mesh, m_CpuTangents);
            AppendSkinningStreams(mesh, m_CpuJoints, m_CpuWeights);
        }

        m_RegisteredMeshCount = totalMeshes;
        m_MeshDataDirty = true;
        FlushMeshUploads(device);
    }

    void ResourceRegistry::SyncMaterials(HedgehogEngine::IResourceCatalog& catalog, RHI::IRHIDevice& device)
    {
        assert(m_MaterialLayout && "SetMaterialLayout must be called before SyncMaterials");

        const FS::FileSystemManager& fileSystem = catalog.GetFileSystem();
        const size_t total = catalog.GetMaterialCount();

        // Every map a material names, so the editor lists it among the known textures.
        const auto registerMaps = [&catalog](const HedgehogEngine::MaterialView& material)
        {
            for (uint32_t slot = 0; slot < MATERIAL_TEXTURE_BINDING_COUNT; ++slot)
                if (const std::string& path = GetMaterialTexturePath(material, static_cast<MaterialTextureBinding>(slot));
                    !path.empty())
                    catalog.RegisterTexturePath(path);
        };

        for (size_t i = 0; i < m_RegisteredMaterialCount && i < total; ++i)
        {
            const HedgehogEngine::MaterialView mat = catalog.GetMaterial(i);
            if (!mat.isDirty)
                continue;

            UpdateMaterialGpu(static_cast<uint32_t>(i), mat, device, fileSystem);
            registerMaps(mat);
            catalog.ClearMaterialDirty(i);
        }

        for (size_t i = m_RegisteredMaterialCount; i < total; ++i)
        {
            const HedgehogEngine::MaterialView mat = catalog.GetMaterial(i);
            CreateMaterialGpu(mat, device, fileSystem);
            registerMaps(mat);
            catalog.ClearMaterialDirty(i);
        }

        m_RegisteredMaterialCount = total;
    }

    void ResourceRegistry::FlushMeshUploads(RHI::IRHIDevice& device)
    {
        if (!m_MeshDataDirty)
            return;

        if (m_PositionsBuffer)
            device.WaitIdle();

        const size_t posSize = m_CpuPositions.size() * sizeof(float);
        const size_t uvSize  = m_CpuTexCoords.size() * sizeof(float);
        const size_t nrmSize = m_CpuNormals.size()   * sizeof(float);
        const size_t tanSize = m_CpuTangents.size()  * sizeof(float);
        const size_t idxSize = m_CpuIndices.size()   * sizeof(uint32_t);
        const size_t jntSize = m_CpuJoints.size()    * sizeof(uint32_t);
        const size_t wgtSize = m_CpuWeights.size()   * sizeof(float);

        m_PositionsBuffer = device.CreateBuffer(posSize, RHI::BufferUsage::VertexBuffer, RHI::MemoryUsage::CpuToGpu);
        m_TexCoordsBuffer = device.CreateBuffer(uvSize,  RHI::BufferUsage::VertexBuffer, RHI::MemoryUsage::CpuToGpu);
        m_NormalsBuffer   = device.CreateBuffer(nrmSize, RHI::BufferUsage::VertexBuffer, RHI::MemoryUsage::CpuToGpu);
        m_TangentsBuffer  = device.CreateBuffer(tanSize, RHI::BufferUsage::VertexBuffer, RHI::MemoryUsage::CpuToGpu);
        m_IndexBuffer     = device.CreateBuffer(idxSize, RHI::BufferUsage::IndexBuffer,  RHI::MemoryUsage::CpuToGpu);
        m_JointsBuffer    = device.CreateBuffer(jntSize, RHI::BufferUsage::VertexBuffer, RHI::MemoryUsage::CpuToGpu);
        m_WeightsBuffer   = device.CreateBuffer(wgtSize, RHI::BufferUsage::VertexBuffer, RHI::MemoryUsage::CpuToGpu);

        m_PositionsBuffer->CopyData(m_CpuPositions.data(), posSize);
        m_TexCoordsBuffer->CopyData(m_CpuTexCoords.data(), uvSize);
        m_NormalsBuffer->CopyData(m_CpuNormals.data(),     nrmSize);
        m_TangentsBuffer->CopyData(m_CpuTangents.data(),   tanSize);
        m_IndexBuffer->CopyData(m_CpuIndices.data(),        idxSize);
        m_JointsBuffer->CopyData(m_CpuJoints.data(),        jntSize);
        m_WeightsBuffer->CopyData(m_CpuWeights.data(),      wgtSize);

        m_MeshDataDirty = false;
    }

    RHI::IRHITexture& ResourceRegistry::GetOrCreateTexture(const std::string& path,
                                                             RHI::IRHIDevice& device,
                                                             const FS::FileSystemManager& fileSystem,
                                                             bool srgb)
    {
        // One copy per file and colour space, however the path is spelt (FS::MakeAssetKey); '|' never
        // appears in a path, so a linear copy never collides with one.
        const std::string file = FS::MakeAssetKey(path);
        const std::string key  = srgb ? file : file + "|linear";
        auto it = m_TextureCache.find(key);
        if (it != m_TextureCache.end())
            return *it->second;

        ContentLoader::TextureLoader loader;
        const bool loaded = loader.LoadTexture(file, fileSystem);
        // A missing/corrupt texture must not take down rendering: substitute a
        // 1x1 magenta placeholder (cached under the same path like any texture).
        constexpr uint8_t FALLBACK_PIXEL[4] = { 255, 0, 255, 255 };
        const uint32_t texW    = loaded ? static_cast<uint32_t>(loader.GetWidth())  : 1u;
        const uint32_t texH    = loaded ? static_cast<uint32_t>(loader.GetHeight()) : 1u;
        // Mipmapped, since materials draw it at any distance.
        auto texture = UploadRgba8(device, loaded ? loader.GetData() : FALLBACK_PIXEL, texW, texH, true, srgb);

        auto [result, _] = m_TextureCache.emplace(key, std::move(texture));
        return *result->second;
    }

    void ResourceRegistry::SetUiTextureLayout(RHI::IRHIDevice& device, const RHI::IRHIDescriptorSetLayout& layout)
    {
        assert(m_UiTextureSets.empty() && "SetUiTextureLayout must be called before any UI texture is synced");
        m_UiTextureLayout = &layout;
        // The textures' and fonts' budgets, and one more: the white solid-fill texture's.
        constexpr uint32_t UI_SETS = MAX_UI_TEXTURE_SETS + MAX_UI_FONT_SETS + 1;
        m_UiTexturePool = device.CreateDescriptorPool(UI_SETS, { { RHI::DescriptorType::CombinedImageSampler, UI_SETS } });

        RHI::SamplerDesc samplerDesc;
        samplerDesc.MinFilter    = RHI::Filter::Linear;
        samplerDesc.MagFilter    = RHI::Filter::Linear;
        samplerDesc.AddressModeU = RHI::AddressMode::ClampToEdge;
        samplerDesc.AddressModeV = RHI::AddressMode::ClampToEdge;
        samplerDesc.AddressModeW = RHI::AddressMode::ClampToEdge;
        m_UiSampler = device.CreateSampler(samplerDesc);
    }

    void ResourceRegistry::SyncUiTextures(std::span<const std::string> paths, RHI::IRHIDevice& device,
                                          const FS::FileSystemManager& fileSystem)
    {
        assert(m_UiTextureLayout && "SetUiTextureLayout must be called before SyncUiTextures");
        if (!m_UiSolidSet)
        {
            constexpr uint8_t WHITE_PIXEL[4] = { 255, 255, 255, 255 };
            m_UiSolidTexture = UploadRgba8(device, WHITE_PIXEL, 1, 1, false);
            m_UiSolidSet     = device.AllocateDescriptorSet(*m_UiTexturePool, *m_UiTextureLayout);
            m_UiSolidSet->WriteTexture(0, *m_UiSolidTexture, *m_UiSampler);
            m_UiSolidSet->Flush();
        }

        for (const std::string& path : paths)
        {
            if (m_UiTextureSpellings.contains(path))
                continue;
            // A new spelling of a file already synced shares its set.
            std::string key = FS::MakeAssetKey(path);
            if (const auto found = m_UiTextureSets.find(key); found != m_UiTextureSets.end())
            {
                m_UiTextureSpellings.emplace(path, found->second.get());
                continue;
            }
            if (m_UiTextureSets.size() >= MAX_UI_TEXTURE_SETS)
            {
                if (!m_WarnedUiTextureLimit)
                    LOGWARNING("ResourceRegistry: more than", MAX_UI_TEXTURE_SETS, "UI textures;", path,
                               "and later ones draw as solid fills.");
                m_WarnedUiTextureLimit = true;
                continue;
            }
            RHI::IRHITexture& texture = GetOrCreateTexture(path, device, fileSystem);
            auto              set     = device.AllocateDescriptorSet(*m_UiTexturePool, *m_UiTextureLayout);
            set->WriteTexture(0, texture, *m_UiSampler);
            set->Flush();
            m_UiTextureSpellings.emplace(path, set.get());
            m_UiTextureSets.emplace(std::move(key), std::move(set));
        }
    }

    void ResourceRegistry::SyncFonts(const HedgehogEngine::IResourceCatalog& catalog, RHI::IRHIDevice& device)
    {
        assert(m_UiTextureLayout && "SetUiTextureLayout must be called before SyncFonts");
        std::vector<uint8_t> pixels;
        for (size_t font = m_FontSets.size(); font < catalog.GetFontCount(); ++font)
        {
            if (font >= MAX_UI_FONT_SETS)
            {
                if (font == MAX_UI_FONT_SETS)
                    LOGWARNING("ResourceRegistry: more than", MAX_UI_FONT_SETS, "baked fonts; text in later ones is not drawn.");
                m_FontTextures.push_back(nullptr);
                m_FontSets.push_back(nullptr);
                continue;
            }

            // White everywhere, the coverage in alpha: the UI shader multiplies the text colour by it.
            const HedgehogEngine::FontAtlasView atlas = catalog.GetFontAtlas(font);
            pixels.assign(atlas.Atlas.size() * 4, 255);
            for (size_t i = 0; i < atlas.Atlas.size(); ++i)
                pixels[i * 4 + 3] = atlas.Atlas[i];
            auto texture = UploadRgba8(device, pixels.data(), atlas.AtlasWidth, atlas.AtlasHeight, false);
            auto set     = device.AllocateDescriptorSet(*m_UiTexturePool, *m_UiTextureLayout);
            set->WriteTexture(0, *texture, *m_UiSampler);
            set->Flush();
            m_FontTextures.push_back(std::move(texture));
            m_FontSets.push_back(std::move(set));
        }
    }

    const RHI::IRHIDescriptorSet* ResourceRegistry::FindUiFontSet(size_t font) const
    {
        return font < m_FontSets.size() ? m_FontSets[font].get() : nullptr;
    }

    const RHI::IRHIDescriptorSet* ResourceRegistry::FindUiTextureSet(const std::string& path) const
    {
        const auto found = m_UiTextureSpellings.find(path);
        return found != m_UiTextureSpellings.end() ? found->second : nullptr;
    }

    void ResourceRegistry::CreateMaterialGpu(const HedgehogEngine::MaterialView& material, RHI::IRHIDevice& device,
                                              const FS::FileSystemManager& fileSystem)
    {
        MaterialGpuData data;
        data.UniformBuffer = device.CreateBuffer(sizeof(MaterialUniform), RHI::BufferUsage::UniformBuffer,
                                                 RHI::MemoryUsage::CpuToGpu);
        data.DescriptorSet = device.AllocateDescriptorSet(*m_MaterialPool, *m_MaterialLayout);
        WriteMaterialSet(data, material, device, fileSystem);
        m_Materials.push_back(std::move(data));
    }

    void ResourceRegistry::UpdateMaterialGpu(uint32_t index, const HedgehogEngine::MaterialView& material,
                                              RHI::IRHIDevice& device, const FS::FileSystemManager& fileSystem)
    {
        WriteMaterialSet(m_Materials[index], material, device, fileSystem);
    }

    void ResourceRegistry::WriteMaterialSet(MaterialGpuData& gpu, const HedgehogEngine::MaterialView& material,
                                            RHI::IRHIDevice& device, const FS::FileSystemManager& fileSystem)
    {
        if (!m_WhiteSrgbTexture)
        {
            constexpr uint8_t WHITE[4]       = { 255, 255, 255, 255 };
            constexpr uint8_t FLAT_NORMAL[4] = { 128, 128, 255, 255 };
            m_WhiteSrgbTexture   = UploadRgba8(device, WHITE, 1, 1, false, true);
            m_WhiteLinearTexture = UploadRgba8(device, WHITE, 1, 1, false, false);
            m_FlatNormalTexture  = UploadRgba8(device, FLAT_NORMAL, 1, 1, false, false);
        }

        const MaterialUniform uniform = MakeMaterialUniform(material);
        gpu.UniformBuffer->CopyData(&uniform, sizeof(uniform));
        gpu.DescriptorSet->WriteUniformBuffer(0, *gpu.UniformBuffer);
        for (uint32_t slot = 0; slot < MATERIAL_TEXTURE_BINDING_COUNT; ++slot)
        {
            const auto         binding = static_cast<MaterialTextureBinding>(slot);
            const bool         srgb    = IsColorTexture(binding);
            const std::string& path    = GetMaterialTexturePath(material, binding);
            RHI::IRHITexture*  texture = nullptr;
            if (!path.empty())
                texture = &GetOrCreateTexture(path, device, fileSystem, srgb);
            else if (binding == MaterialTextureBinding::Normal)
                texture = m_FlatNormalTexture.get();
            else
                texture = srgb ? m_WhiteSrgbTexture.get() : m_WhiteLinearTexture.get();
            gpu.DescriptorSet->WriteTexture(1 + slot, *texture, *m_LinearSampler);
        }
        gpu.DescriptorSet->Flush();
    }

    const MeshGeometryInfo& ResourceRegistry::GetMeshGeometryInfo(size_t meshIndex) const
    {
        return m_MeshGeometryInfos[meshIndex];
    }

    const RHI::IRHIBuffer& ResourceRegistry::GetPositionsBuffer() const { return *m_PositionsBuffer; }
    const RHI::IRHIBuffer& ResourceRegistry::GetTexCoordsBuffer() const { return *m_TexCoordsBuffer; }
    const RHI::IRHIBuffer& ResourceRegistry::GetNormalsBuffer()   const { return *m_NormalsBuffer;   }
    const RHI::IRHIBuffer& ResourceRegistry::GetTangentsBuffer()  const { return *m_TangentsBuffer;  }
    const RHI::IRHIBuffer& ResourceRegistry::GetIndexBuffer()     const { return *m_IndexBuffer;     }
    const RHI::IRHIBuffer& ResourceRegistry::GetJointsBuffer()    const { return *m_JointsBuffer;    }
    const RHI::IRHIBuffer& ResourceRegistry::GetWeightsBuffer()   const { return *m_WeightsBuffer;   }

    const RHI::IRHIDescriptorSet& ResourceRegistry::GetMaterialDescriptorSet(uint32_t index) const
    {
        return *m_Materials[index].DescriptorSet;
    }

    void ResourceRegistry::Cleanup(RHI::IRHIDevice& device)
    {
        device.WaitIdle();

        m_Materials.clear();       // descriptor sets freed before pool
        m_UiTextureSpellings.clear();
        m_UiTextureSets.clear();
        m_FontSets.clear();
        m_FontTextures.clear();
        m_UiSolidSet.reset();
        m_UiSolidTexture.reset();
        m_UiTexturePool.reset();
        m_UiSampler.reset();
        m_UiTextureLayout = nullptr;
        m_TextureCache.clear();
        m_WhiteSrgbTexture.reset();
        m_WhiteLinearTexture.reset();
        m_FlatNormalTexture.reset();
        m_LinearSampler.reset();
        m_MaterialPool.reset();
        m_MaterialLayout = nullptr;

        m_WeightsBuffer.reset();
        m_JointsBuffer.reset();
        m_IndexBuffer.reset();
        m_TangentsBuffer.reset();
        m_NormalsBuffer.reset();
        m_TexCoordsBuffer.reset();
        m_PositionsBuffer.reset();
    }
}
