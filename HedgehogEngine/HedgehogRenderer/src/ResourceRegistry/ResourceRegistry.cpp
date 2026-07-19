#include "ResourceRegistry.hpp"

#include "ContentLoader/api/TextureLoader.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHIBuffer.hpp"
#include "RHI/api/IRHITexture.hpp"
#include "RHI/api/IRHISampler.hpp"
#include "RHI/api/IRHIDescriptor.hpp"
#include "RHI/api/IRHICommandList.hpp"

#include <cassert>

namespace HR
{
    ResourceRegistry::ResourceRegistry(RHI::IRHIDevice& device)
    {
        RHI::SamplerDesc samplerDesc;
        samplerDesc.m_MinFilter    = RHI::Filter::Linear;
        samplerDesc.m_MagFilter    = RHI::Filter::Linear;
        samplerDesc.m_AddressModeU = RHI::AddressMode::Repeat;
        samplerDesc.m_AddressModeV = RHI::AddressMode::Repeat;
        samplerDesc.m_AddressModeW = RHI::AddressMode::Repeat;
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
            geom.m_FirstIndex   = mesh.firstIndex;
            geom.m_IndexCount   = mesh.indexCount;
            geom.m_VertexOffset = mesh.vertexOffset;
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

        m_PositionsBuffer = device.CreateBuffer(posSize, RHI::BufferUsage::VertexBuffer, RHI::MemoryUsage::CpuToGpu);
        m_TexCoordsBuffer = device.CreateBuffer(uvSize,  RHI::BufferUsage::VertexBuffer, RHI::MemoryUsage::CpuToGpu);
        m_NormalsBuffer   = device.CreateBuffer(nrmSize, RHI::BufferUsage::VertexBuffer, RHI::MemoryUsage::CpuToGpu);
        m_IndexBuffer     = device.CreateBuffer(idxSize, RHI::BufferUsage::IndexBuffer,  RHI::MemoryUsage::CpuToGpu);

        m_PositionsBuffer->CopyData(m_CpuPositions.data(), posSize);
        m_TexCoordsBuffer->CopyData(m_CpuTexCoords.data(), uvSize);
        m_NormalsBuffer->CopyData(m_CpuNormals.data(),     nrmSize);
        m_IndexBuffer->CopyData(m_CpuIndices.data(),        idxSize);

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
        const size_t   imgSize = texW * texH * 4;

        auto staging = device.CreateBuffer(imgSize, RHI::BufferUsage::TransferSrc, RHI::MemoryUsage::CpuToGpu);
        staging->CopyData(loaded ? loader.GetData() : FALLBACK_PIXEL, imgSize);

        RHI::TextureDesc desc;
        desc.m_Width  = texW;
        desc.m_Height = texH;
        desc.m_Format = RHI::Format::R8G8B8A8Srgb;
        desc.m_Usage  = RHI::TextureUsage::Sampled | RHI::TextureUsage::TransferDst;
        auto texture  = device.CreateTexture(desc);

        device.ExecuteImmediately([&](RHI::IRHICommandList& cmd)
        {
            cmd.TransitionTexture(*texture, RHI::ImageLayout::Undefined, RHI::ImageLayout::TransferDst);
            cmd.CopyBufferToTexture(*staging, *texture);
            cmd.TransitionTexture(*texture, RHI::ImageLayout::TransferDst, RHI::ImageLayout::ShaderReadOnly);
        });

        auto [result, _] = m_TextureCache.emplace(path, std::move(texture));
        return *result->second;
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
        data.m_UniformBuffer = std::move(ubo);
        data.m_DescriptorSet = std::move(set);
        m_Materials.push_back(std::move(data));
    }

    void ResourceRegistry::UpdateMaterialGpu(uint32_t index, float transparency,
                                              const std::string& texturePath, RHI::IRHIDevice& device,
                                              const FS::FileSystemManager& fileSystem)
    {
        MaterialUniform uniform{ transparency };
        m_Materials[index].m_UniformBuffer->CopyData(&uniform, sizeof(uniform));

        auto& texture = GetOrCreateTexture(texturePath, device, fileSystem);
        m_Materials[index].m_DescriptorSet->WriteUniformBuffer(0, *m_Materials[index].m_UniformBuffer);
        m_Materials[index].m_DescriptorSet->WriteTexture(1, texture, *m_LinearSampler);
        m_Materials[index].m_DescriptorSet->Flush();
    }

    const MeshGeometryInfo& ResourceRegistry::GetMeshGeometryInfo(size_t meshIndex) const
    {
        return m_MeshGeometryInfos[meshIndex];
    }

    const RHI::IRHIBuffer& ResourceRegistry::GetPositionsBuffer() const { return *m_PositionsBuffer; }
    const RHI::IRHIBuffer& ResourceRegistry::GetTexCoordsBuffer() const { return *m_TexCoordsBuffer; }
    const RHI::IRHIBuffer& ResourceRegistry::GetNormalsBuffer()   const { return *m_NormalsBuffer;   }
    const RHI::IRHIBuffer& ResourceRegistry::GetIndexBuffer()     const { return *m_IndexBuffer;     }

    const RHI::IRHIDescriptorSet& ResourceRegistry::GetMaterialDescriptorSet(uint32_t index) const
    {
        return *m_Materials[index].m_DescriptorSet;
    }

    void ResourceRegistry::Cleanup(RHI::IRHIDevice& device)
    {
        device.WaitIdle();

        m_Materials.clear();       // descriptor sets freed before pool
        m_TextureCache.clear();
        m_LinearSampler.reset();
        m_MaterialPool.reset();
        m_MaterialLayout = nullptr;

        m_IndexBuffer.reset();
        m_NormalsBuffer.reset();
        m_TexCoordsBuffer.reset();
        m_PositionsBuffer.reset();
    }
}
