#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

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

        const RHI::IRHITexture& GetRHITexture(const VulkanContext& context, std::string filePath) const;
        const RHI::IRHISampler& GetRHISampler(const VulkanContext& context, SamplerType type) const;

        const std::vector<std::string>& GetTexturePathes() const;
        size_t GetTextureIndex(std::string name) const;

        void Cleanup(const VulkanContext& context);

    private:
        const RHI::IRHITexture& CreateRHITexture(const VulkanContext& context, std::string filePath) const;
        const RHI::IRHISampler& CreateRHISampler(const VulkanContext& context, SamplerType type) const;

    private:
        mutable std::vector<std::string>                              m_TexturePathes;
        mutable std::unordered_map<std::string, std::unique_ptr<RHI::IRHITexture>> m_RHIImages;
        mutable std::unordered_map<SamplerType,  std::unique_ptr<RHI::IRHISampler>> m_RHISamplersList;
    };

}
