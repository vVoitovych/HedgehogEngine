#pragma once

#include "RHI/api/RHITypes.hpp"

#include <memory>
#include <string>
#include <unordered_map>

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

        void SyncResources(RHI::IRHIDevice& device, HedgehogEngine::EngineContext& engine);

        // Called at the start and end of each frame — hooks for future transient ring-buffer.
        void BeginFrame(uint32_t frameIndex);
        void EndFrame();

        void ResizeFrameBufferSizeDependenteResources(RHI::IRHIDevice& device,
                                                      const RHI::IRHISwapchain& swapchain);
        void ResizeSceneView(RHI::IRHIDevice& device, uint32_t width, uint32_t height);
        void ResizeSettingsDependenteResources(RHI::IRHIDevice& device,
                                               HedgehogSettings::Settings& settings);

        // Persistent textures — recreated only on explicit resize events.
        const RHI::IRHITexture& GetRHIColorBuffer() const;
        const RHI::IRHITexture& GetRHIShadowMap()   const;
        const RHI::IRHITexture& GetRHIShadowMask()  const;

        // Transient textures — graph-managed, re-registered each Compile().
        const RHI::IRHITexture& GetRHIDepthBuffer()   const;
        const RHI::IRHITexture& GetSceneColorBuffer() const;

              HR::ResourceRegistry& GetResourceRegistry();
        const HR::ResourceRegistry& GetResourceRegistry() const;

    private:
        // Persistent texture helpers.
        void CreateRHIColorBuffer(RHI::IRHIDevice& device, const RHI::IRHISwapchain& swapchain);
        void CreateRHIShadowMap(RHI::IRHIDevice& device, uint32_t shadowmapSize);
        void CreateRHIShadowMask(RHI::IRHIDevice& device, const RHI::IRHISwapchain& swapchain);

        // Transient texture helpers.
        struct TransientEntry
        {
            RHI::TextureDesc                  m_Desc;
            std::unique_ptr<RHI::IRHITexture> m_Texture;
        };

        void RegisterTransient(RHI::IRHIDevice& device,
                                const std::string& name,
                                const RHI::TextureDesc& desc);
        void RecreateTransient(RHI::IRHIDevice& device,
                                const std::string& name,
                                const RHI::TextureDesc& desc);

    private:
        // Persistent textures.
        std::unique_ptr<RHI::IRHITexture> m_RHIColorBuffer;
        std::unique_ptr<RHI::IRHITexture> m_RHIShadowMap;
        std::unique_ptr<RHI::IRHITexture> m_RHIShadowMask;

        // Transient textures keyed by name ("RHIDepthBuffer", "SceneColorBuffer", …).
        std::unordered_map<std::string, TransientEntry> m_Transients;

        std::unique_ptr<HR::ResourceRegistry> m_ResourceRegistry;
    };
}
