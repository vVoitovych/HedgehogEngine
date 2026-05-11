#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace RHI
{
    class IRHIDevice;
    class IRHISwapchain;
    class IRHITexture;
    class IRHIBuffer;
    struct TextureDesc;
    enum class BufferUsage : uint32_t;
    enum class MemoryUsage;
}

namespace Renderer
{
    class ResourceManager
    {
    public:
        // m_Swapchain is stored as a reference — safe because RHIContext::RecreateSwapchain
        // calls IRHISwapchain::Resize() in-place; the swapchain object is never replaced.
        ResourceManager(const RHI::IRHISwapchain& swapchain);
        ~ResourceManager();

        ResourceManager(const ResourceManager&)            = delete;
        ResourceManager& operator=(const ResourceManager&) = delete;
        ResourceManager(ResourceManager&&)                 = delete;
        ResourceManager& operator=(ResourceManager&&)      = delete;

        void Cleanup(RHI::IRHIDevice& device);

        // Call each frame with the index returned by AcquireNextImage.
        // After this, GetTexture(ResourceNames::SWAP_CHAIN_BACK_BUFFER) returns
        // the correct swapchain image for the current frame.
        void SetBackBufferIndex(uint32_t index);

        // ── Texture API ──────────────────────────────────────────────────────
        // Asserts if name already exists or name == SWAP_CHAIN_BACK_BUFFER.
        void CreateTexture(std::string_view        name,
                           RHI::IRHIDevice&        device,
                           const RHI::TextureDesc& desc);

        // No-op if name does not exist. Asserts if name == SWAP_CHAIN_BACK_BUFFER.
        void DestroyTexture(std::string_view name);

        bool HasTexture(std::string_view name) const;

        // Asserts if name is not found.
        // SWAP_CHAIN_BACK_BUFFER resolves to the swapchain image at the current index.
        const RHI::IRHITexture& GetTexture(std::string_view name) const;
              RHI::IRHITexture& GetTexture(std::string_view name);

        // ── Buffer API ───────────────────────────────────────────────────────
        // Asserts if name already exists.
        void CreateBuffer(std::string_view name,
                          RHI::IRHIDevice& device,
                          size_t           size,
                          RHI::BufferUsage usage,
                          RHI::MemoryUsage memUsage);

        // No-op if name does not exist.
        void DestroyBuffer(std::string_view name);

        bool HasBuffer(std::string_view name) const;

        // Asserts if name is not found.
        const RHI::IRHIBuffer& GetBuffer(std::string_view name) const;
              RHI::IRHIBuffer& GetBuffer(std::string_view name);

    private:
        std::unordered_map<std::string, std::unique_ptr<RHI::IRHITexture>> m_Textures;
        std::unordered_map<std::string, std::unique_ptr<RHI::IRHIBuffer>>  m_Buffers;

        const RHI::IRHISwapchain& m_Swapchain;
        uint32_t                  m_BackBufferIndex = 0;
    };
}
