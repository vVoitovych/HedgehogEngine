#include "ResourceManager.hpp"

#include "HedgehogRenderer/ResourceRegistry/ResourceRegistry.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Containers/MeshContainer.hpp"
#include "HedgehogEngine/api/Containers/MaterialContainer.hpp"
#include "HedgehogEngine/api/Containers/TextureContainer.hpp"
#include "HedgehogSettings/Settings/HedgehogSettings.hpp"
#include "HedgehogSettings/Settings/ShadowmapingSettings.hpp"

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

    void ResourceManager::SyncResources(RHI::IRHIDevice& device, HedgehogEngine::EngineContext& engine)
    {
        m_ResourceRegistry->SyncMeshes(engine.GetMeshContainer(), device);
        m_ResourceRegistry->SyncMaterials(engine.GetMaterialContainer(), engine.GetTextureContainer(), device);
    }

    void ResourceManager::ResizeFrameBufferSizeDependenteResources(RHI::IRHIDevice& device,
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
        m_RHIDepthBuffer.reset();
        m_SceneColorBuffer.reset();

        RHI::TextureDesc depthDesc;
        depthDesc.m_Width  = width;
        depthDesc.m_Height = height;
        depthDesc.m_Format = device.GetPreferredDepthFormat();
        depthDesc.m_Usage  = RHI::TextureUsage::DepthStencil;
        m_RHIDepthBuffer = device.CreateTexture(depthDesc);

        RHI::TextureDesc colorDesc;
        colorDesc.m_Width  = width;
        colorDesc.m_Height = height;
        colorDesc.m_Format = RHI::Format::R16G16B16A16Unorm;
        colorDesc.m_Usage  = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled;
        m_SceneColorBuffer = device.CreateTexture(colorDesc);
    }

    void ResourceManager::ResizeSettingsDependenteResources(RHI::IRHIDevice& device,
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
        desc.m_Width  = swapchain.GetWidth();
        desc.m_Height = swapchain.GetHeight();
        desc.m_Format = RHI::Format::R16G16B16A16Unorm;
        desc.m_Usage  = RHI::TextureUsage::ColorAttachment
                      | RHI::TextureUsage::TransferSrc
                      | RHI::TextureUsage::TransferDst
                      | RHI::TextureUsage::Storage;

        m_RHIColorBuffer = device.CreateTexture(desc);
        LOGINFO("RHI color buffer created");
    }

    void ResourceManager::CreateRHIDepthBuffer(RHI::IRHIDevice& device, const RHI::IRHISwapchain& swapchain)
    {
        RHI::TextureDesc desc;
        desc.m_Width  = swapchain.GetWidth();
        desc.m_Height = swapchain.GetHeight();
        desc.m_Format = device.GetPreferredDepthFormat();
        desc.m_Usage  = RHI::TextureUsage::DepthStencil;

        m_RHIDepthBuffer = device.CreateTexture(desc);
        LOGINFO("RHI depth buffer created");
    }

    void ResourceManager::CreateRHIShadowMap(RHI::IRHIDevice& device, uint32_t shadowmapSize)
    {
        RHI::TextureDesc desc;
        desc.m_Width  = shadowmapSize;
        desc.m_Height = shadowmapSize;
        desc.m_Format = device.GetPreferredDepthFormat();
        desc.m_Usage  = RHI::TextureUsage::DepthStencil;

        m_RHIShadowMap = device.CreateTexture(desc);
        LOGINFO("RHI shadow map created");
    }

    void ResourceManager::CreateRHIShadowMask(RHI::IRHIDevice& device, const RHI::IRHISwapchain& swapchain)
    {
        RHI::TextureDesc desc;
        desc.m_Width  = swapchain.GetWidth();
        desc.m_Height = swapchain.GetHeight();
        desc.m_Format = RHI::Format::R16Float;
        desc.m_Usage  = RHI::TextureUsage::Sampled | RHI::TextureUsage::Storage;

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
        desc.m_Width  = swapchain.GetWidth();
        desc.m_Height = swapchain.GetHeight();
        desc.m_Format = RHI::Format::R16G16B16A16Unorm;
        desc.m_Usage  = RHI::TextureUsage::ColorAttachment
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
