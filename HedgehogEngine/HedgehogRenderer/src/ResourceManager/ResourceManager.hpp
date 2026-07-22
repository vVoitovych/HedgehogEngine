#pragma once

#include <cstdint>
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

namespace HedgehogEngine
{
    class IResourceCatalog;
}

namespace HR
{
    class ResourceRegistry;
}

namespace Renderer
{
    // Off-screen viewport targets. Scene = editor flycam view, Game = ECS primary-camera view.
    // Used to index per-target framebuffers/uniforms in the geometry passes.
    enum class RenderTargetId : uint32_t { Scene = 0, Game = 1 };
    inline constexpr uint32_t kRenderTargetCount = 2;

    class ResourceManager
    {
    public:
        ResourceManager(RHI::IRHIDevice& device,
                        const RHI::IRHISwapchain& swapchain,
                        const HedgehogSettings::Settings& settings);
        ~ResourceManager();

        void Cleanup(RHI::IRHIDevice& device);

        void SyncResources(RHI::IRHIDevice& device, HedgehogEngine::IResourceCatalog& catalog);

        void ResizeFrameBufferSizeDependentResources(RHI::IRHIDevice& device,
                                                      const RHI::IRHISwapchain& swapchain);
        void ResizeSceneView(RHI::IRHIDevice& device, uint32_t width, uint32_t height);
        void ResizeGameView(RHI::IRHIDevice& device, uint32_t width, uint32_t height);
        void ResizeSettingsDependentResources(RHI::IRHIDevice& device,
                                               HedgehogSettings::Settings& settings);

        const RHI::IRHITexture& GetRHIColorBuffer() const;
        const RHI::IRHITexture& GetRHIDepthBuffer() const;
        const RHI::IRHITexture& GetRHIShadowMap() const;
        const RHI::IRHITexture& GetRHIShadowMask() const;
        const RHI::IRHITexture& GetSceneColorBuffer() const;

        // Per-target attachments; the geometry passes render into these by RenderTargetId.
        const RHI::IRHITexture& GetColorBuffer(RenderTargetId target) const;
        const RHI::IRHITexture& GetDepthBuffer(RenderTargetId target) const;

              HR::ResourceRegistry& GetResourceRegistry();
        const HR::ResourceRegistry& GetResourceRegistry() const;

    private:
        void CreateRHIColorBuffer(RHI::IRHIDevice& device, const RHI::IRHISwapchain& swapchain);
        void CreateRHIDepthBuffer(RHI::IRHIDevice& device, const RHI::IRHISwapchain& swapchain);
        void CreateRHIShadowMap(RHI::IRHIDevice& device, uint32_t shadowmapSize);
        void CreateRHIShadowMask(RHI::IRHIDevice& device, const RHI::IRHISwapchain& swapchain);
        void CreateSceneColorBuffer(RHI::IRHIDevice& device, const RHI::IRHISwapchain& swapchain);
        void CreateGameViewBuffers(RHI::IRHIDevice& device, const RHI::IRHISwapchain& swapchain);

    private:
        std::unique_ptr<RHI::IRHITexture> m_RHIDepthBuffer;
        std::unique_ptr<RHI::IRHITexture> m_RHIColorBuffer;
        std::unique_ptr<RHI::IRHITexture> m_RHIShadowMap;
        std::unique_ptr<RHI::IRHITexture> m_RHIShadowMask;
        std::unique_ptr<RHI::IRHITexture> m_SceneColorBuffer;
        std::unique_ptr<RHI::IRHITexture> m_GameColorBuffer;
        std::unique_ptr<RHI::IRHITexture> m_GameDepthBuffer;

        std::unique_ptr<HR::ResourceRegistry> m_ResourceRegistry;
    };
}
