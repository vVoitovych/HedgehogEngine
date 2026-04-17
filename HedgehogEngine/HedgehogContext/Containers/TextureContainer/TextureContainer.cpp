#include "TextureContainer.hpp"

#include "HedgehogContext/Context/VulkanContext.hpp"

#include "ContentLoader/api/TextureLoader.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHICommandList.hpp"

#include "Logger/api/Logger.hpp"

#include <stdexcept>

namespace Context
{
    TextureContainer::TextureContainer()
    {
    }

    TextureContainer::~TextureContainer()
    {
    }

    void TextureContainer::Cleanup(const VulkanContext& context)
    {
        context.GetRHIDevice().WaitIdle();
        m_RHIImages.clear();
        m_RHISamplersList.clear();
        m_TexturePathes.clear();
    }

    const RHI::IRHITexture& TextureContainer::CreateRHITexture(const VulkanContext& context, std::string filePath) const
    {
        ContentLoader::TextureLoader textureLoader;
        textureLoader.LoadTexture(filePath);
        uint32_t texWidth  = static_cast<uint32_t>(textureLoader.GetWidth());
        uint32_t texHeight = static_cast<uint32_t>(textureLoader.GetHeight());
        size_t imageSize   = texWidth * texHeight * 4;

        auto& rhiDevice = context.GetRHIDevice();

        auto stagingBuffer = rhiDevice.CreateBuffer(
            imageSize, RHI::BufferUsage::TransferSrc, RHI::MemoryUsage::CpuToGpu);
        stagingBuffer->CopyData(textureLoader.GetData(), imageSize);

        RHI::TextureDesc desc;
        desc.m_Width  = texWidth;
        desc.m_Height = texHeight;
        desc.m_Format = RHI::Format::R8G8B8A8Srgb;
        desc.m_Usage  = RHI::TextureUsage::Sampled | RHI::TextureUsage::TransferDst;

        auto texture = rhiDevice.CreateTexture(desc);

        rhiDevice.ExecuteImmediately([&](RHI::IRHICommandList& cmd)
        {
            cmd.TransitionTexture(*texture, RHI::ImageLayout::Undefined, RHI::ImageLayout::TransferDst);
            cmd.CopyBufferToTexture(*stagingBuffer, *texture);
            cmd.TransitionTexture(*texture, RHI::ImageLayout::TransferDst, RHI::ImageLayout::ShaderReadOnly);
        });

        auto [it, _] = m_RHIImages.emplace(filePath, std::move(texture));
        m_TexturePathes.push_back(filePath);
        LOGINFO("RHI texture ", filePath, " loaded and initialized");
        return *it->second;
    }

    const RHI::IRHISampler& TextureContainer::CreateRHISampler(const VulkanContext& context, SamplerType type) const
    {
        if (type == SamplerType::Linear)
        {
            RHI::SamplerDesc desc;
            desc.m_MinFilter    = RHI::Filter::Linear;
            desc.m_MagFilter    = RHI::Filter::Linear;
            desc.m_AddressModeU = RHI::AddressMode::Repeat;
            desc.m_AddressModeV = RHI::AddressMode::Repeat;
            desc.m_AddressModeW = RHI::AddressMode::Repeat;
            auto sampler = context.GetRHIDevice().CreateSampler(desc);
            auto [it, _] = m_RHISamplersList.emplace(type, std::move(sampler));
            return *it->second;
        }
        throw std::runtime_error("unsupported sampler type");
    }

    const RHI::IRHITexture& TextureContainer::GetRHITexture(const VulkanContext& context, std::string filePath) const
    {
        auto it = m_RHIImages.find(filePath);
        if (it != m_RHIImages.end())
            return *it->second;
        return CreateRHITexture(context, filePath);
    }

    const RHI::IRHISampler& TextureContainer::GetRHISampler(const VulkanContext& context, SamplerType type) const
    {
        auto it = m_RHISamplersList.find(type);
        if (it != m_RHISamplersList.end())
            return *it->second;
        return CreateRHISampler(context, type);
    }

    const std::vector<std::string>& TextureContainer::GetTexturePathes() const
    {
        return m_TexturePathes;
    }

    size_t TextureContainer::GetTextureIndex(std::string name) const
    {
        auto it = std::find(m_TexturePathes.begin(), m_TexturePathes.end(), name);
        return it - m_TexturePathes.begin();
    }

}
