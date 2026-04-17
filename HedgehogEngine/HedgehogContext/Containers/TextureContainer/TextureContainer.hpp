#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wrappers
{
    class Device;
    class Image;
    class Sampler;
}

namespace RHI
{
    class IRHITexture;
    class IRHISampler;
}

namespace Context
{
    class VulkanContext;

    enum class SamplerType
    {
        Linear
    };

    class TextureContainer
    {
    public:
        TextureContainer();
        ~TextureContainer();

        TextureContainer(const TextureContainer&) = delete;
        TextureContainer(TextureContainer&&) = delete;
        TextureContainer& operator=(const TextureContainer&) = delete;
        TextureContainer& operator=(TextureContainer&&) = delete;

        // Legacy Wrappers API (kept until all callers are migrated).
        const Wrappers::Image&   GetImage(const Wrappers::Device& device, std::string filePath) const;
        const Wrappers::Sampler& GetSampler(const Wrappers::Device& device, SamplerType type) const;

        // New RHI API.
        const RHI::IRHITexture& GetRHITexture(const VulkanContext& context, std::string filePath) const;
        const RHI::IRHISampler& GetRHISampler(const VulkanContext& context, SamplerType type) const;

        const std::vector<std::string>& GetTexturePathes() const;
        size_t GetTextureIndex(std::string name) const;

        void Cleanup(const VulkanContext& context);

    private:
        const Wrappers::Image&   CreateImage(const Wrappers::Device& device, std::string filePath) const;
        const Wrappers::Sampler& CreateSampler(const Wrappers::Device& device, SamplerType type) const;

        const RHI::IRHITexture& CreateRHITexture(const VulkanContext& context, std::string filePath) const;
        const RHI::IRHISampler& CreateRHISampler(const VulkanContext& context, SamplerType type) const;

    private:
        // Legacy Wrappers objects (removed once all callers are migrated).
        mutable std::unordered_map<SamplerType, Wrappers::Sampler> m_SamplersList;
        mutable std::unordered_map<std::string, Wrappers::Image>   m_Images;
        mutable std::vector<std::string>                            m_TexturePathes;

        // RHI objects — authoritative going forward.
        mutable std::unordered_map<std::string,  std::unique_ptr<RHI::IRHITexture>> m_RHIImages;
        mutable std::unordered_map<SamplerType,  std::unique_ptr<RHI::IRHISampler>> m_RHISamplersList;
    };



}






