#include "MaterialSync.hpp"

#include "ContentLoader/api/TextureLoader.hpp"

#include "HedgehogEngine/api/Containers/MaterialContainer.hpp"
#include "HedgehogEngine/api/Containers/MaterialData.hpp"
#include "HedgehogEngine/api/Containers/TextureContainer.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHIBuffer.hpp"
#include "RHI/api/IRHITexture.hpp"
#include "RHI/api/IRHISampler.hpp"
#include "RHI/api/IRHIDescriptor.hpp"
#include "RHI/api/IRHICommandList.hpp"

#include <cassert>

namespace Renderer
{
    void MaterialSync::SetMaterialLayout(RHI::IRHIDevice&                    device,
                                         const RHI::IRHIDescriptorSetLayout& layout,
                                         uint32_t                            maxSets,
                                         const std::vector<RHI::PoolSize>&   poolSizes)
    {
        assert(m_Materials.empty() && "SetMaterialLayout must be called before any materials are registered");

        if (!m_LinearSampler)
        {
            RHI::SamplerDesc samplerDesc;
            samplerDesc.m_MinFilter    = RHI::Filter::Linear;
            samplerDesc.m_MagFilter    = RHI::Filter::Linear;
            samplerDesc.m_AddressModeU = RHI::AddressMode::Repeat;
            samplerDesc.m_AddressModeV = RHI::AddressMode::Repeat;
            samplerDesc.m_AddressModeW = RHI::AddressMode::Repeat;
            m_LinearSampler = device.CreateSampler(samplerDesc);
        }

        m_MaterialLayout = &layout;
        m_MaterialPool   = device.CreateDescriptorPool(maxSets, poolSizes);
    }

    void MaterialSync::Sync(HedgehogEngine::MaterialContainer& container,
                             HedgehogEngine::TextureContainer&  texContainer,
                             RHI::IRHIDevice&                   device)
    {
        assert(m_MaterialLayout && "SetMaterialLayout must be called before Sync");

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

    const RHI::IRHIDescriptorSet& MaterialSync::GetMaterialDescriptorSet(uint32_t index) const
    {
        return *m_Materials[index].m_DescriptorSet;
    }

    void MaterialSync::Cleanup(RHI::IRHIDevice& device)
    {
        device.WaitIdle();

        m_Materials.clear();
        m_TextureCache.clear();
        m_LinearSampler.reset();
        m_MaterialPool.reset();
        m_MaterialLayout = nullptr;
        m_RegisteredMaterialCount = 0;
    }

    RHI::IRHITexture& MaterialSync::GetOrCreateTexture(const std::string& path, RHI::IRHIDevice& device)
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

    void MaterialSync::CreateMaterialGpu(float transparency, const std::string& texturePath,
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

    void MaterialSync::UpdateMaterialGpu(uint32_t index, float transparency,
                                          const std::string& texturePath, RHI::IRHIDevice& device)
    {
        MaterialUniform uniform{ transparency };
        m_Materials[index].m_UniformBuffer->CopyData(&uniform, sizeof(uniform));

        auto& texture = GetOrCreateTexture(texturePath, device);
        m_Materials[index].m_DescriptorSet->WriteUniformBuffer(0, *m_Materials[index].m_UniformBuffer);
        m_Materials[index].m_DescriptorSet->WriteTexture(1, texture, *m_LinearSampler);
        m_Materials[index].m_DescriptorSet->Flush();
    }
}
