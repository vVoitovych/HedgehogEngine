#include "TextureContainer.hpp"

#include "HedgehogContext/Context/VulkanContext.hpp"
#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"
#include "HedgehogWrappers/Wrappeers/Resources/Image/Image.hpp"
#include "HedgehogWrappers/Wrappeers/Resources/Buffer/Buffer.hpp"
#include "HedgehogWrappers/Wrappeers/Resources/Sampler/Sampler.hpp"
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
        auto& device = context.GetDevice();

        for (auto& image : m_Images)
        {
            image.second.Cleanup(device);
        }
        m_Images.clear();
        m_TexturePathes.clear();

        for (auto& sampler : m_SamplersList)
        {
            sampler.second.Cleanup(device);
        }
        m_SamplersList.clear();

        // RHI resources self-destruct via unique_ptr; wait for idle before clearing.
        context.GetRHIDevice().WaitIdle();
        m_RHIImages.clear();
        m_RHISamplersList.clear();
    }

    const Wrappers::Image& TextureContainer::CreateImage(const Wrappers::Device& device, std::string filePath) const
    {
        ContentLoader::TextureLoader textureLoader;
        textureLoader.LoadTexture(filePath);
        int texWidth = textureLoader.GetWidth();
        int texHeight = textureLoader.GetHeight();
        VkDeviceSize imageSize = static_cast<VkDeviceSize>(texWidth * texHeight * 4);

        Wrappers::Buffer stageBuffer(
            device,
            imageSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU);

        stageBuffer.CopyDataToBufferMemory(device, textureLoader.GetData(), static_cast<size_t>(imageSize));

        VkFormat format = VK_FORMAT_R8G8B8A8_SRGB; //TODO: make different texture formats
        Wrappers::Image image(
            device,
            texWidth, texHeight,
            format,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        device.TransitionImageLayout(image.GetNativeImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        device.CopyBufferToImage(stageBuffer.GetNativeBuffer(), image.GetNativeImage(), static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
        device.TransitionImageLayout(image.GetNativeImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        stageBuffer.DestroyBuffer(device);
        image.CreateImageView(device, format, VK_IMAGE_ASPECT_COLOR_BIT);

        auto result = m_Images.emplace(filePath, std::move(image));
        m_TexturePathes.push_back(filePath);

        LOGINFO("Vulkan texture ", filePath, " loaded and initialized");
        auto it = m_Images.find(filePath);
        return it->second;
    }

    const Wrappers::Sampler& TextureContainer::CreateSampler(const Wrappers::Device& device, SamplerType type) const
    {
        if (type== Context::SamplerType::Linear)
        {
            Wrappers::Sampler linearSampler(device);
            m_SamplersList.emplace(SamplerType::Linear, std::move(linearSampler));
            auto it = m_SamplersList.find(type);
            return it->second;
        }
        else
        {
            throw std::runtime_error("unsupported sampler type");
        }
    }

    const Wrappers::Image& TextureContainer::GetImage(const Wrappers::Device& device, std::string filePath) const
    {
        auto it = m_Images.find(filePath);
        if (it != m_Images.end())
        {
            return it->second;
        }
        else
        {
            return CreateImage(device, filePath);
        }
    }

    const Wrappers::Sampler& TextureContainer::GetSampler(const Wrappers::Device& device, SamplerType type) const
    {
        auto it = m_SamplersList.find(type);
        if (it != m_SamplersList.end())
        {
            return it->second;
        }
        else
        {
            return CreateSampler(device, type);
        }

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



