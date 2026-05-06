#include "ResourceManager.hpp"
#include "ResourceNames.hpp"

#include "ResourceRegistry/ResourceRegistry.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Containers/MeshContainer.hpp"
#include "HedgehogEngine/api/Containers/MaterialContainer.hpp"
#include "HedgehogEngine/api/Containers/TextureContainer.hpp"
#include "HedgehogSettings/Settings/HedgehogSettings.hpp"
#include "HedgehogSettings/Settings/ShadowmapingSettings.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHISwapchain.hpp"
#include "RHI/api/IRHITexture.hpp"

#include "Logger/api/Logger.hpp"

#include <cassert>

namespace Renderer
{
    ResourceManager::ResourceManager(RHI::IRHIDevice& device,
                                     const RHI::IRHISwapchain& swapchain,
                                     const HedgehogSettings::Settings& settings)
    {
        m_ResourceRegistry = std::make_unique<HR::ResourceRegistry>(device);

        {
            RHI::TextureDesc desc;
            desc.m_Width  = swapchain.GetWidth();
            desc.m_Height = swapchain.GetHeight();
            desc.m_Format = RHI::Format::R16G16B16A16Unorm;
            desc.m_Usage  = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::TransferSrc
                          | RHI::TextureUsage::TransferDst     | RHI::TextureUsage::Storage;
            CreateTexture(ResourceNames::RHIColorBuffer, desc, device);
        }
        {
            RHI::TextureDesc desc;
            desc.m_Width  = swapchain.GetWidth();
            desc.m_Height = swapchain.GetHeight();
            desc.m_Format = device.GetPreferredDepthFormat();
            desc.m_Usage  = RHI::TextureUsage::DepthStencil;
            CreateTexture(ResourceNames::RHIDepthBuffer, desc, device);
        }
        {
            const uint32_t sz = settings.GetShadowmapSettings()->GetShadowmapSize();
            RHI::TextureDesc desc;
            desc.m_Width  = sz;
            desc.m_Height = sz;
            desc.m_Format = device.GetPreferredDepthFormat();
            desc.m_Usage  = RHI::TextureUsage::DepthStencil;
            CreateTexture(ResourceNames::RHIShadowMap, desc, device);
        }
        {
            RHI::TextureDesc desc;
            desc.m_Width  = swapchain.GetWidth();
            desc.m_Height = swapchain.GetHeight();
            desc.m_Format = RHI::Format::R16Float;
            desc.m_Usage  = RHI::TextureUsage::Sampled | RHI::TextureUsage::Storage;
            CreateTexture(ResourceNames::RHIShadowMask, desc, device);
        }
        {
            RHI::TextureDesc desc;
            desc.m_Width  = swapchain.GetWidth();
            desc.m_Height = swapchain.GetHeight();
            desc.m_Format = RHI::Format::R16G16B16A16Unorm;
            desc.m_Usage  = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled;
            CreateTexture(ResourceNames::SceneColorBuffer, desc, device);
        }
    }

    ResourceManager::~ResourceManager() = default;

    void ResourceManager::Cleanup(RHI::IRHIDevice& device)
    {
        device.WaitIdle();
        m_Textures.clear();
        m_ResourceRegistry->Cleanup(device);
    }

    void ResourceManager::SyncResources(RHI::IRHIDevice& device, HedgehogEngine::EngineContext& engine)
    {
        m_ResourceRegistry->SyncMeshes(engine.GetMeshContainer(), device);
        m_ResourceRegistry->SyncMaterials(engine.GetMaterialContainer(), engine.GetTextureContainer(), device);
    }

    void ResourceManager::ResizeFrameBufferSizeDependenteResources(RHI::IRHIDevice& device,
                                                                    const RHI::IRHISwapchain& swapchain)
    {
        device.WaitIdle();

        DestroyTexture(ResourceNames::RHIColorBuffer);
        DestroyTexture(ResourceNames::RHIShadowMask);

        {
            RHI::TextureDesc desc;
            desc.m_Width  = swapchain.GetWidth();
            desc.m_Height = swapchain.GetHeight();
            desc.m_Format = RHI::Format::R16G16B16A16Unorm;
            desc.m_Usage  = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::TransferSrc
                          | RHI::TextureUsage::TransferDst     | RHI::TextureUsage::Storage;
            CreateTexture(ResourceNames::RHIColorBuffer, desc, device);
        }
        {
            RHI::TextureDesc desc;
            desc.m_Width  = swapchain.GetWidth();
            desc.m_Height = swapchain.GetHeight();
            desc.m_Format = RHI::Format::R16Float;
            desc.m_Usage  = RHI::TextureUsage::Sampled | RHI::TextureUsage::Storage;
            CreateTexture(ResourceNames::RHIShadowMask, desc, device);
        }
    }

    void ResourceManager::ResizeSceneView(RHI::IRHIDevice& device, uint32_t width, uint32_t height)
    {
        DestroyTexture(ResourceNames::RHIDepthBuffer);
        DestroyTexture(ResourceNames::SceneColorBuffer);

        {
            RHI::TextureDesc desc;
            desc.m_Width  = width;
            desc.m_Height = height;
            desc.m_Format = device.GetPreferredDepthFormat();
            desc.m_Usage  = RHI::TextureUsage::DepthStencil;
            CreateTexture(ResourceNames::RHIDepthBuffer, desc, device);
        }
        {
            RHI::TextureDesc desc;
            desc.m_Width  = width;
            desc.m_Height = height;
            desc.m_Format = RHI::Format::R16G16B16A16Unorm;
            desc.m_Usage  = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled;
            CreateTexture(ResourceNames::SceneColorBuffer, desc, device);
        }
    }

    void ResourceManager::ResizeSettingsDependenteResources(RHI::IRHIDevice& device,
                                                             HedgehogSettings::Settings& settings)
    {
        auto& shadowmapSettings = settings.GetShadowmapSettings();
        if (!shadowmapSettings->IsDirty())
            return;

        device.WaitIdle();

        DestroyTexture(ResourceNames::RHIShadowMap);

        const uint32_t sz = shadowmapSettings->GetShadowmapSize();
        RHI::TextureDesc desc;
        desc.m_Width  = sz;
        desc.m_Height = sz;
        desc.m_Format = device.GetPreferredDepthFormat();
        desc.m_Usage  = RHI::TextureUsage::DepthStencil;
        CreateTexture(ResourceNames::RHIShadowMap, desc, device);

        shadowmapSettings->CleanDirtyState();
    }

    void ResourceManager::CreateTexture(const std::string& name, const RHI::TextureDesc& desc,
                                         RHI::IRHIDevice& device)
    {
        assert(m_Textures.find(name) == m_Textures.end() && "Texture already registered");
        m_Textures[name] = device.CreateTexture(desc);
        LOGINFO("Texture created: ", name);
    }

    void ResourceManager::DestroyTexture(const std::string& name)
    {
        auto it = m_Textures.find(name);
        assert(it != m_Textures.end() && "Texture not found");
        m_Textures.erase(it);
    }

    RHI::IRHITexture& ResourceManager::GetTexture(const std::string& name)
    {
        auto it = m_Textures.find(name);
        assert(it != m_Textures.end() && "Texture not found");
        return *it->second;
    }

    const RHI::IRHITexture& ResourceManager::GetTexture(const std::string& name) const
    {
        auto it = m_Textures.find(name);
        assert(it != m_Textures.end() && "Texture not found");
        return *it->second;
    }

    bool ResourceManager::HasTexture(const std::string& name) const
    {
        return m_Textures.find(name) != m_Textures.end();
    }

    HR::ResourceRegistry& ResourceManager::GetResourceRegistry()
    {
        return *m_ResourceRegistry;
    }

    const HR::ResourceRegistry& ResourceManager::GetResourceRegistry() const
    {
        return *m_ResourceRegistry;
    }
}
