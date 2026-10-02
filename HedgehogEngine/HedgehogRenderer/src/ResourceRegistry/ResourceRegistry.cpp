#include "ResourceRegistry.hpp"
#include "SkinningStreams.hpp"

#include "ContentLoader/api/TextureLoader.hpp"

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
        // A sampled R8G8B8A8Srgb texture holding width x height pixels, uploaded and ready to read.
        std::unique_ptr<RHI::IRHITexture> UploadRgba8(RHI::IRHIDevice& device, const void* pixels, uint32_t width,
                                                      uint32_t height)
        {
            const size_t imgSize = static_cast<size_t>(width) * height * 4;
            auto staging = device.CreateBuffer(imgSize, RHI::BufferUsage::TransferSrc, RHI::MemoryUsage::CpuToGpu);
            staging->CopyData(pixels, imgSize);

            RHI::TextureDesc desc;
            desc.Width  = width;
            desc.Height = height;
            desc.Format = RHI::Format::R8G8B8A8Srgb;
            desc.Usage  = RHI::TextureUsage::Sampled | RHI::TextureUsage::TransferDst;
            auto texture = device.CreateTexture(desc);

            device.ExecuteImmediately([&](RHI::IRHICommandList& cmd)
            {
                const RHI::TextureBarrier toCopy{ texture.get(), RHI::ResourceState::Undefined, RHI::ResourceState::CopyDst };
                cmd.Barrier({ &toCopy, 1 }, {});
                cmd.CopyBufferToTexture(*staging, *texture);
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

        for (size_t i = 0; i < m_RegisteredMaterialCount && i < total; ++i)
        {
            const HedgehogEngine::MaterialView mat = catalog.GetMaterial(i);
            if (!mat.isDirty)
                continue;

            UpdateMaterialGpu(static_cast<uint32_t>(i), mat.transparency, mat.baseColor,
                              device, fileSystem);
            catalog.RegisterTexturePath(mat.baseColor);
            catalog.ClearMaterialDirty(i);
        }

        for (size_t i = m_RegisteredMaterialCount; i < total; ++i)
        {
            const HedgehogEngine::MaterialView mat = catalog.GetMaterial(i);
            CreateMaterialGpu(mat.transparency, mat.baseColor, device, fileSystem);
            catalog.RegisterTexturePath(mat.baseColor);
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
        const size_t idxSize = m_CpuIndices.size()   * sizeof(uint32_t);
        const size_t jntSize = m_CpuJoints.size()    * sizeof(uint32_t);
        const size_t wgtSize = m_CpuWeights.size()   * sizeof(float);

        m_PositionsBuffer = device.CreateBuffer(posSize, RHI::BufferUsage::VertexBuffer, RHI::MemoryUsage::CpuToGpu);
        m_TexCoordsBuffer = device.CreateBuffer(uvSize,  RHI::BufferUsage::VertexBuffer, RHI::MemoryUsage::CpuToGpu);
        m_NormalsBuffer   = device.CreateBuffer(nrmSize, RHI::BufferUsage::VertexBuffer, RHI::MemoryUsage::CpuToGpu);
        m_IndexBuffer     = device.CreateBuffer(idxSize, RHI::BufferUsage::IndexBuffer,  RHI::MemoryUsage::CpuToGpu);
        m_JointsBuffer    = device.CreateBuffer(jntSize, RHI::BufferUsage::VertexBuffer, RHI::MemoryUsage::CpuToGpu);
        m_WeightsBuffer   = device.CreateBuffer(wgtSize, RHI::BufferUsage::VertexBuffer, RHI::MemoryUsage::CpuToGpu);

        m_PositionsBuffer->CopyData(m_CpuPositions.data(), posSize);
        m_TexCoordsBuffer->CopyData(m_CpuTexCoords.data(), uvSize);
        m_NormalsBuffer->CopyData(m_CpuNormals.data(),     nrmSize);
        m_IndexBuffer->CopyData(m_CpuIndices.data(),        idxSize);
        m_JointsBuffer->CopyData(m_CpuJoints.data(),        jntSize);
        m_WeightsBuffer->CopyData(m_CpuWeights.data(),      wgtSize);

        m_MeshDataDirty = false;
    }

    RHI::IRHITexture& ResourceRegistry::GetOrCreateTexture(const std::string& path,
                                                             RHI::IRHIDevice& device,
                                                             const FS::FileSystemManager& fileSystem)
    {
        auto it = m_TextureCache.find(path);
        if (it != m_TextureCache.end())
            return *it->second;

        ContentLoader::TextureLoader loader;
        const bool loaded = loader.LoadTexture(path, fileSystem);
        // A missing/corrupt texture must not take down rendering: substitute a
        // 1x1 magenta placeholder (cached under the same path like any texture).
        constexpr uint8_t FALLBACK_PIXEL[4] = { 255, 0, 255, 255 };
        const uint32_t texW    = loaded ? static_cast<uint32_t>(loader.GetWidth())  : 1u;
        const uint32_t texH    = loaded ? static_cast<uint32_t>(loader.GetHeight()) : 1u;
        auto texture = UploadRgba8(device, loaded ? loader.GetData() : FALLBACK_PIXEL, texW, texH);

        auto [result, _] = m_TextureCache.emplace(path, std::move(texture));
        return *result->second;
    }

    void ResourceRegistry::SetUiTextureLayout(RHI::IRHIDevice& device, const RHI::IRHIDescriptorSetLayout& layout)
    {
        assert(m_UiTextureSets.empty() && "SetUiTextureLayout must be called before any UI texture is synced");
        m_UiTextureLayout = &layout;
        // One more set than the budget: the white solid-fill texture's.
        m_UiTexturePool = device.CreateDescriptorPool(
            MAX_UI_TEXTURE_SETS + 1, { { RHI::DescriptorType::CombinedImageSampler, MAX_UI_TEXTURE_SETS + 1 } });

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
            m_UiSolidTexture = UploadRgba8(device, WHITE_PIXEL, 1, 1);
            m_UiSolidSet     = device.AllocateDescriptorSet(*m_UiTexturePool, *m_UiTextureLayout);
            m_UiSolidSet->WriteTexture(0, *m_UiSolidTexture, *m_UiSampler);
            m_UiSolidSet->Flush();
        }

        for (const std::string& path : paths)
        {
            if (m_UiTextureSets.contains(path))
                continue;
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
            m_UiTextureSets.emplace(path, std::move(set));
        }
    }

    const RHI::IRHIDescriptorSet* ResourceRegistry::FindUiTextureSet(const std::string& path) const
    {
        const auto found = m_UiTextureSets.find(path);
        return found != m_UiTextureSets.end() ? found->second.get() : nullptr;
    }

    void ResourceRegistry::CreateMaterialGpu(float transparency, const std::string& texturePath,
                                              RHI::IRHIDevice& device,
                                              const FS::FileSystemManager& fileSystem)
    {
        MaterialUniform uniform{ transparency };
        auto ubo = device.CreateBuffer(sizeof(MaterialUniform), RHI::BufferUsage::UniformBuffer,
                                       RHI::MemoryUsage::CpuToGpu);
        ubo->CopyData(&uniform, sizeof(uniform));

        auto& texture = GetOrCreateTexture(texturePath, device, fileSystem);

        auto set = device.AllocateDescriptorSet(*m_MaterialPool, *m_MaterialLayout);
        set->WriteUniformBuffer(0, *ubo);
        set->WriteTexture(1, texture, *m_LinearSampler);
        set->Flush();

        MaterialGpuData data;
        data.UniformBuffer = std::move(ubo);
        data.DescriptorSet = std::move(set);
        m_Materials.push_back(std::move(data));
    }

    void ResourceRegistry::UpdateMaterialGpu(uint32_t index, float transparency,
                                              const std::string& texturePath, RHI::IRHIDevice& device,
                                              const FS::FileSystemManager& fileSystem)
    {
        MaterialUniform uniform{ transparency };
        m_Materials[index].UniformBuffer->CopyData(&uniform, sizeof(uniform));

        auto& texture = GetOrCreateTexture(texturePath, device, fileSystem);
        m_Materials[index].DescriptorSet->WriteUniformBuffer(0, *m_Materials[index].UniformBuffer);
        m_Materials[index].DescriptorSet->WriteTexture(1, texture, *m_LinearSampler);
        m_Materials[index].DescriptorSet->Flush();
    }

    const MeshGeometryInfo& ResourceRegistry::GetMeshGeometryInfo(size_t meshIndex) const
    {
        return m_MeshGeometryInfos[meshIndex];
    }

    const RHI::IRHIBuffer& ResourceRegistry::GetPositionsBuffer() const { return *m_PositionsBuffer; }
    const RHI::IRHIBuffer& ResourceRegistry::GetTexCoordsBuffer() const { return *m_TexCoordsBuffer; }
    const RHI::IRHIBuffer& ResourceRegistry::GetNormalsBuffer()   const { return *m_NormalsBuffer;   }
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
        m_UiTextureSets.clear();
        m_UiSolidSet.reset();
        m_UiSolidTexture.reset();
        m_UiTexturePool.reset();
        m_UiSampler.reset();
        m_UiTextureLayout = nullptr;
        m_TextureCache.clear();
        m_LinearSampler.reset();
        m_MaterialPool.reset();
        m_MaterialLayout = nullptr;

        m_WeightsBuffer.reset();
        m_JointsBuffer.reset();
        m_IndexBuffer.reset();
        m_NormalsBuffer.reset();
        m_TexCoordsBuffer.reset();
        m_PositionsBuffer.reset();
    }
}
