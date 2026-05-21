#pragma once

#include "RenderGraphTypes.hpp"

#include <cstdint>

namespace HedgehogEngine
{
    struct FrameData;
}

namespace RHI
{
    class IRHIDevice;
    class IRHISwapchain;
    class IRHICommandList;
    class IRHIFence;
    class IRHISemaphore;
    class IRHITexture;
}

namespace Renderer
{
    class ResourceManager;

    // Per-frame execution context passed to every IRenderNode::Execute().
    // Holds non-owning references to all runtime state assembled by Renderer::DrawFrame().
    // backBufferIndex is written by InitNode and read by PresentNode.
    class RenderContext
    {
    public:
        RenderContext(const HedgehogEngine::FrameData& frameData,
                      RHI::IRHIDevice&                 device,
                      RHI::IRHISwapchain&              swapchain,
                      RHI::IRHICommandList&            cmd,
                      RHI::IRHIFence&                  fence,
                      RHI::IRHISemaphore&              imageAvailableSemaphore,
                      RHI::IRHISemaphore&              renderFinishedSemaphore,
                      uint32_t                         frameIndex,
                      const ResourceManager&           resourceManager,
                      const TextureRegistry&           textureRegistry)
            : m_FrameData(frameData)
            , m_Device(device)
            , m_Swapchain(swapchain)
            , m_CommandList(cmd)
            , m_Fence(fence)
            , m_ImageAvailableSemaphore(imageAvailableSemaphore)
            , m_RenderFinishedSemaphore(renderFinishedSemaphore)
            , m_FrameIndex(frameIndex)
            , m_ResourceManager(resourceManager)
            , m_TextureRegistry(textureRegistry)
        {}

        const HedgehogEngine::FrameData& GetFrameData()                 const { return m_FrameData; }
        RHI::IRHIDevice&                 GetDevice()                    const { return m_Device; }
        RHI::IRHISwapchain&              GetSwapchain()                 const { return m_Swapchain; }
        RHI::IRHICommandList&            GetCommandList()               const { return m_CommandList; }
        RHI::IRHIFence&                  GetFence()                     const { return m_Fence; }
        RHI::IRHISemaphore&              GetImageAvailableSemaphore()   const { return m_ImageAvailableSemaphore; }
        RHI::IRHISemaphore&              GetRenderFinishedSemaphore()   const { return m_RenderFinishedSemaphore; }
        uint32_t                         GetFrameIndex()                const { return m_FrameIndex; }
        const ResourceManager&           GetResourceManager()           const { return m_ResourceManager; }

        // Returns the named texture from the registry, or nullptr if not found.
        const RHI::IRHITexture* GetTexture(const std::string& name) const
        {
            auto it = m_TextureRegistry.find(name);
            return it != m_TextureRegistry.end() ? it->second : nullptr;
        }

        uint32_t GetBackBufferIndex()            const { return m_BackBufferIndex; }
        void     SetBackBufferIndex(uint32_t idx)      { m_BackBufferIndex = idx; }

    private:
        const HedgehogEngine::FrameData& m_FrameData;
        RHI::IRHIDevice&                 m_Device;
        RHI::IRHISwapchain&              m_Swapchain;
        RHI::IRHICommandList&            m_CommandList;
        RHI::IRHIFence&                  m_Fence;
        RHI::IRHISemaphore&              m_ImageAvailableSemaphore;
        RHI::IRHISemaphore&              m_RenderFinishedSemaphore;
        uint32_t                         m_FrameIndex      = 0;
        const ResourceManager&           m_ResourceManager;
        const TextureRegistry&           m_TextureRegistry;
        uint32_t                         m_BackBufferIndex = 0;
    };
}
