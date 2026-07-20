#include "ResourceManager.hpp"

#include "ResourceRegistry/ResourceRegistry.hpp"

#include "HedgehogCommon/api/Resource/IResourceCatalog.hpp"
#include "HedgehogSettings/api/HedgehogSettings.hpp"
#include "HedgehogSettings/api/ShadowmapingSettings.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHISwapchain.hpp"
#include "RHI/api/IRHITexture.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"

#include "Logger/api/Logger.hpp"

namespace Renderer
{
    ResourceManager::ResourceManager(RHI::IRHIDevice& device,
                                     const RHI::IRHISwapchain& swapchain,
                                     const HedgehogSettings::Settings& settings)
    {
        m_ResourceRegistry = std::make_unique<HR::ResourceRegistry>(device);

        CreateRHIColorBuffer(device, swapchain);
        CreateRHIDepthBuffer(device, swapchain);
        CreateRHIShadowMap(device, settings.GetShadowmapSettings()->GetShadowmapSize());
        CreateRHIShadowMask(device, swapchain);
        CreateSceneColorBuffer(device, swapchain);
    }

    ResourceManager::~ResourceManager()
    {
    }

    void ResourceManager::Cleanup(RHI::IRHIDevice& device)
    {
        device.WaitIdle();

        m_RHIColorBuffer.reset();
        m_RHIDepthBuffer.reset();
        m_RHIShadowMap.reset();
        m_RHIShadowMask.reset();
        m_SceneColorBuffer.reset();
        m_ResourceRegistry->Cleanup(device);
    }

    void ResourceManager::SyncResources(RHI::IRHIDevice& device, HedgehogEngine::IResourceCatalog& catalog)
    {
        m_ResourceRegistry->SyncMeshes(catalog, device);
        m_ResourceRegistry->SyncMaterials(catalog, device);
    }

    void ResourceManager::ResizeFrameBufferSizeDependentResources(RHI::IRHIDevice& device,
                                                                    const RHI::IRHISwapchain& swapchain)
    {
        device.WaitIdle();
        m_RHIColorBuffer.reset();
        m_RHIShadowMask.reset();

        CreateRHIColorBuffer(device, swapchain);
        CreateRHIShadowMask(device, swapchain);
        // SceneColorBuffer and DepthBuffer are panel-size driven; resized via ResizeSceneView.
    }

    void ResourceManager::ResizeSceneView(RHI::IRHIDevice& device, uint32_t width, uint32_t height)
    {
        device.WaitIdle();
        m_RHIDepthBuffer.reset();
        m_SceneColorBuffer.reset();

        RHI::TextureDesc depthDesc;
        depthDesc.Width  = width;
        depthDesc.Height = height;
        depthDesc.Format = device.GetPreferredDepthFormat();
        depthDesc.Usage  = RHI::TextureUsage::DepthStencil;
        m_RHIDepthBuffer = device.CreateTexture(depthDesc);

        RHI::TextureDesc colorDesc;
        colorDesc.Width  = width;
        colorDesc.Height = height;
        colorDesc.Format = RHI::Format::R16G16B16A16Unorm;
        colorDesc.Usage  = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled;
        m_SceneColorBuffer = device.CreateTexture(colorDesc);
    }

    void ResourceManager::ResizeSettingsDependentResources(RHI::IRHIDevice& device,
                                                             HedgehogSettings::Settings& settings)
    {
        auto& shadowmapSettings = settings.GetShadowmapSettings();

        if (shadowmapSettings->IsDirty())
        {
            device.WaitIdle();
            m_RHIShadowMap.reset();
            CreateRHIShadowMap(device, shadowmapSettings->GetShadowmapSize());

            shadowmapSettings->CleanDirtyState();
        }
    }

    void ResourceManager::CreateRHIColorBuffer(RHI::IRHIDevice& device, const RHI::IRHISwapchain& swapchain)
    {
        RHI::TextureDesc desc;
        desc.Width  = swapchain.GetWidth();
        desc.Height = swapchain.GetHeight();
        desc.Format = RHI::Format::R16G16B16A16Unorm;
        desc.Usage  = RHI::TextureUsage::ColorAttachment
                      | RHI::TextureUsage::TransferSrc
                      | RHI::TextureUsage::TransferDst
                      | RHI::TextureUsage::Storage;

        m_RHIColorBuffer = device.CreateTexture(desc);
        LOGINFO("RHI color buffer created");
    }

    void ResourceManager::CreateRHIDepthBuffer(RHI::IRHIDevice& device, const RHI::IRHISwapchain& swapchain)
    {
        RHI::TextureDesc desc;
        desc.Width  = swapchain.GetWidth();
        desc.Height = swapchain.GetHeight();
        desc.Format = device.GetPreferredDepthFormat();
        desc.Usage  = RHI::TextureUsage::DepthStencil;

        m_RHIDepthBuffer = device.CreateTexture(desc);
        LOGINFO("RHI depth buffer created");
    }

    void ResourceManager::CreateRHIShadowMap(RHI::IRHIDevice& device, uint32_t shadowmapSize)
    {
        RHI::TextureDesc desc;
        desc.Width  = shadowmapSize;
        desc.Height = shadowmapSize;
        desc.Format = device.GetPreferredDepthFormat();
        desc.Usage  = RHI::TextureUsage::DepthStencil;

        m_RHIShadowMap = device.CreateTexture(desc);
        LOGINFO("RHI shadow map created");
    }

    void ResourceManager::CreateRHIShadowMask(RHI::IRHIDevice& device, const RHI::IRHISwapchain& swapchain)
    {
        RHI::TextureDesc desc;
        desc.Width  = swapchain.GetWidth();
        desc.Height = swapchain.GetHeight();
        desc.Format = RHI::Format::R16Float;
        desc.Usage  = RHI::TextureUsage::Sampled | RHI::TextureUsage::Storage;

        m_RHIShadowMask = device.CreateTexture(desc);
        LOGINFO("RHI shadow mask created");
    }

    const RHI::IRHITexture& ResourceManager::GetRHIColorBuffer() const
    {
        return *m_RHIColorBuffer;
    }

    const RHI::IRHITexture& ResourceManager::GetRHIDepthBuffer() const
    {
        return *m_RHIDepthBuffer;
    }

    const RHI::IRHITexture& ResourceManager::GetRHIShadowMap() const
    {
        return *m_RHIShadowMap;
    }

    const RHI::IRHITexture& ResourceManager::GetRHIShadowMask() const
    {
        return *m_RHIShadowMask;
    }

    const RHI::IRHITexture& ResourceManager::GetSceneColorBuffer() const
    {
        return *m_SceneColorBuffer;
    }

    void ResourceManager::CreateSceneColorBuffer(RHI::IRHIDevice& device, const RHI::IRHISwapchain& swapchain)
    {
        RHI::TextureDesc desc;
        desc.Width  = swapchain.GetWidth();
        desc.Height = swapchain.GetHeight();
        desc.Format = RHI::Format::R16G16B16A16Unorm;
        desc.Usage  = RHI::TextureUsage::ColorAttachment
                      | RHI::TextureUsage::Sampled;

        m_SceneColorBuffer = device.CreateTexture(desc);
        LOGINFO("Scene color buffer created");
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
