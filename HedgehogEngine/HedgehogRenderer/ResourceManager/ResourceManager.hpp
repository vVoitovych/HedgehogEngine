#pragma once

#include <memory>

namespace RHI
{
    class IRHIDevice;
    class IRHISwapchain;
    class IRHITexture;
}

namespace HedgehogSettings
{
    class Settings;
}

namespace Context
{
    class EngineContext;
}

namespace HR
{
    class ResourceRegistry;
}

namespace Renderer
{
    class ResourceManager
    {
    public:
        ResourceManager(RHI::IRHIDevice& device,
                        const RHI::IRHISwapchain& swapchain,
                        const HedgehogSettings::Settings& settings);
        ~ResourceManager();

        void Cleanup(RHI::IRHIDevice& device);

        void SyncResources(RHI::IRHIDevice& device, Context::EngineContext& engine);

        void ResizeFrameBufferSizeDependenteResources(RHI::IRHIDevice& device,
                                                      const RHI::IRHISwapchain& swapchain);
        void ResizeSettingsDependenteResources(RHI::IRHIDevice& device,
                                               HedgehogSettings::Settings& settings);

        const RHI::IRHITexture& GetRHIColorBuffer() const;
        const RHI::IRHITexture& GetRHIDepthBuffer() const;
        const RHI::IRHITexture& GetRHIShadowMap() const;
        const RHI::IRHITexture& GetRHIShadowMask() const;
        const RHI::IRHITexture& GetSceneColorBuffer() const;

              HR::ResourceRegistry& GetResourceRegistry();
        const HR::ResourceRegistry& GetResourceRegistry() const;

    private:
        void CreateRHIColorBuffer(RHI::IRHIDevice& device, const RHI::IRHISwapchain& swapchain);
        void CreateRHIDepthBuffer(RHI::IRHIDevice& device, const RHI::IRHISwapchain& swapchain);
        void CreateRHIShadowMap(RHI::IRHIDevice& device, uint32_t shadowmapSize);
        void CreateRHIShadowMask(RHI::IRHIDevice& device, const RHI::IRHISwapchain& swapchain);
        void CreateSceneColorBuffer(RHI::IRHIDevice& device, const RHI::IRHISwapchain& swapchain);

    private:
        std::unique_ptr<RHI::IRHITexture> m_RHIDepthBuffer;
        std::unique_ptr<RHI::IRHITexture> m_RHIColorBuffer;
        std::unique_ptr<RHI::IRHITexture> m_RHIShadowMap;
        std::unique_ptr<RHI::IRHITexture> m_RHIShadowMask;
        std::unique_ptr<RHI::IRHITexture> m_SceneColorBuffer;

        std::unique_ptr<HR::ResourceRegistry> m_ResourceRegistry;
    };
}
