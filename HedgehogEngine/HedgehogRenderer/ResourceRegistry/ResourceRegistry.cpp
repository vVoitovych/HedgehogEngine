#include "ResourceRegistry.hpp"

#include "ContentLoader/api/TextureLoader.hpp"

#include "HedgehogContext/Containers/MeshContainer/MeshContainer.hpp"
#include "HedgehogContext/Containers/MeshContainer/Mesh.hpp"
#include "HedgehogContext/Containers/MaterialContainer/MaterialContainer.hpp"
#include "HedgehogContext/Containers/MaterialContainer/MaterialData.hpp"
#include "HedgehogContext/Containers/TextureContainer/TextureContainer.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHIBuffer.hpp"
#include "RHI/api/IRHITexture.hpp"
#include "RHI/api/IRHISampler.hpp"
#include "RHI/api/IRHIDescriptor.hpp"
#include "RHI/api/IRHICommandList.hpp"

namespace HR
{
    ResourceRegistry::ResourceRegistry(RHI::IRHIDevice& device)
    {
        m_MaterialLayout = device.CreateDescriptorSetLayout({
            { 0, RHI::DescriptorType::UniformBuffer,        1, RHI::ShaderStage::Fragment },
            { 1, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Fragment },
        });

        m_MaterialPool = device.CreateDescriptorPool(
            MAX_MATERIAL_COUNT,
            {
                { RHI::DescriptorType::UniformBuffer,        MAX_MATERIAL_COUNT },
                { RHI::DescriptorType::CombinedImageSampler, MAX_MATERIAL_COUNT },
            });

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

    void ResourceRegistry::SyncMeshes(const Context::MeshContainer& container, RHI::IRHIDevice& device)
    {
        const size_t totalMeshes = container.GetMeshCount();
        if (totalMeshes <= m_RegisteredMeshCount)
            return;

        for (size_t i = m_RegisteredMeshCount; i < totalMeshes; ++i)
        {
            const auto& mesh     = container.GetMesh(i);
            const auto positions = mesh.GetPositions();
            const auto texCoords = mesh.GetTexCoords();
            const auto normals   = mesh.GetNormals();
            const auto indices   = mesh.GetIndices();

            MeshGeometryInfo geom;
            geom.m_FirstIndex   = mesh.GetFirstIndex();
            geom.m_IndexCount   = mesh.GetIndexCount();
            geom.m_VertexOffset = mesh.GetVertexOffset();
            m_MeshGeometryInfos.push_back(geom);

            for (const auto& p : positions)
            {
                m_CpuPositions.push_back(p.x());
                m_CpuPositions.push_back(p.y());
                m_CpuPositions.push_back(p.z());
            }
            for (const auto& uv : texCoords)
            {
                m_CpuTexCoords.push_back(uv.x());
                m_CpuTexCoords.push_back(uv.y());
            }
            for (const auto& n : normals)
            {
                m_CpuNormals.push_back(n.x());
                m_CpuNormals.push_back(n.y());
                m_CpuNormals.push_back(n.z());
            }
            for (uint32_t idx : indices)
                m_CpuIndices.push_back(idx);
        }

        m_RegisteredMeshCount = totalMeshes;
        m_MeshDataDirty = true;
        FlushMeshUploads(device);
    }

    void ResourceRegistry::SyncMaterials(Context::MaterialContainer& container,
                                          Context::TextureContainer&  texContainer,
                                          RHI::IRHIDevice&            device)
    {
        const size_t total = container.GetMaterialCount();

        for (size_t i = 0; i < m_RegisteredMaterialCount && i < total; ++i)
        {
            auto& mat = container.GetMaterialDataByIndex(i);
            if (!mat.isDirty)
                continue;

            UpdateMaterialGpu(static_cast<uint32_t>(i), mat.transparency, mat.baseColor, device);
            texContainer.RegisterTexturePath(mat.baseColor);
            mat.isDirty = false;
        }

        for (size_t i = m_RegisteredMaterialCount; i < total; ++i)
        {
            auto& mat = container.GetMaterialDataByIndex(i);
            CreateMaterialGpu(mat.transparency, mat.baseColor, device);
            texContainer.RegisterTexturePath(mat.baseColor);
            mat.isDirty = false;
        }

        m_RegisteredMaterialCount = total;
    }

    void ResourceRegistry::FlushMeshUploads(RHI::IRHIDevice& device)
    {
        if (!m_MeshDataDirty)
            return;

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

    RHI::IRHITexture& ResourceRegistry::GetOrCreateTexture(const std::string& path, RHI::IRHIDevice& device)
    {
        auto it = m_TextureCache.find(path);
        if (it != m_TextureCache.end())
            return *it->second;

        ContentLoader::TextureLoader loader;
        loader.LoadTexture(path);
        const uint32_t texW    = static_cast<uint32_t>(loader.GetWidth());
        const uint32_t texH    = static_cast<uint32_t>(loader.GetHeight());
        const size_t   imgSize = texW * texH * 4;

        auto staging = device.CreateBuffer(imgSize, RHI::BufferUsage::TransferSrc, RHI::MemoryUsage::CpuToGpu);
        staging->CopyData(loader.GetData(), imgSize);

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
                                              RHI::IRHIDevice& device)
    {
        MaterialUniform uniform{ transparency };
        auto ubo = device.CreateBuffer(sizeof(MaterialUniform), RHI::BufferUsage::UniformBuffer,
                                       RHI::MemoryUsage::CpuToGpu);
        ubo->CopyData(&uniform, sizeof(uniform));

        auto& texture = GetOrCreateTexture(texturePath, device);

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
                                              const std::string& texturePath, RHI::IRHIDevice& device)
    {
        MaterialUniform uniform{ transparency };
        m_Materials[index].m_UniformBuffer->CopyData(&uniform, sizeof(uniform));

        auto& texture = GetOrCreateTexture(texturePath, device);
        m_Materials[index].m_DescriptorSet->WriteUniformBuffer(0, *m_Materials[index].m_UniformBuffer);
        m_Materials[index].m_DescriptorSet->WriteTexture(1, texture, *m_LinearSampler);
        m_Materials[index].m_DescriptorSet->Flush();
    }

    const MeshGeometryInfo& ResourceRegistry::GetMeshGeometryInfo(size_t meshIndex) const
    {
        return m_MeshGeometryInfos[meshIndex];
    }

    const RHI::IRHIBuffer& ResourceRegistry::GetPositionsBuffer() const
    {
        return *m_PositionsBuffer;
    }

    const RHI::IRHIBuffer& ResourceRegistry::GetTexCoordsBuffer() const
    {
        return *m_TexCoordsBuffer;
    }

    const RHI::IRHIBuffer& ResourceRegistry::GetNormalsBuffer() const
    {
        return *m_NormalsBuffer;
    }

    const RHI::IRHIBuffer& ResourceRegistry::GetIndexBuffer() const
    {
        return *m_IndexBuffer;
    }

    const RHI::IRHIDescriptorSetLayout& ResourceRegistry::GetMaterialDescriptorSetLayout() const
    {
        return *m_MaterialLayout;
    }

    const RHI::IRHIDescriptorSet& ResourceRegistry::GetMaterialDescriptorSet(uint32_t index) const
    {
        return *m_Materials[index].m_DescriptorSet;
    }

    void ResourceRegistry::Cleanup(RHI::IRHIDevice& device)
    {
        device.WaitIdle();

        m_Materials.clear();
        m_TextureCache.clear();
        m_LinearSampler.reset();
        m_MaterialPool.reset();
        m_MaterialLayout.reset();

        m_IndexBuffer.reset();
        m_NormalsBuffer.reset();
        m_TexCoordsBuffer.reset();
        m_PositionsBuffer.reset();
    }
}
