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

        void ResizeFrameBufferSizeDependenteResources(RHI::IRHIDevice& device,
                                                      const RHI::IRHISwapchain& swapchain);
        void ResizeSceneView(RHI::IRHIDevice& device, uint32_t width, uint32_t height);
        void ResizeSettingsDependenteResources(RHI::IRHIDevice& device,
                                               HedgehogSettings::Settings& settings);

        void                  CreateTexture(const std::string& name, const RHI::TextureDesc& desc,
                                            RHI::IRHIDevice& device);
        void                  DestroyTexture(const std::string& name);
        RHI::IRHITexture&     GetTexture(const std::string& name);
        const RHI::IRHITexture& GetTexture(const std::string& name) const;
        bool                  HasTexture(const std::string& name) const;

              HR::ResourceRegistry& GetResourceRegistry();
        const HR::ResourceRegistry& GetResourceRegistry() const;

    private:
        std::unordered_map<std::string, std::unique_ptr<RHI::IRHITexture>> m_Textures;
        std::unique_ptr<HR::ResourceRegistry>                               m_ResourceRegistry;
    };
}
