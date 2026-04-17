#include "ResourceManager.hpp"

#include "HedgehogContext/Context/Context.hpp"
#include "HedgehogContext/Context/VulkanContext.hpp"
#include "HedgehogContext/Context/EngineContext.hpp"
#include "HedgehogSettings/Settings/HedgehogSettings.hpp"
#include "HedgehogSettings/Settings/ShadowmapingSettings.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHISwapchain.hpp"
#include "RHI/api/IRHITexture.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"

#include "Logger/api/Logger.hpp"

namespace Renderer
{
    ResourceManager::ResourceManager(const Context::Context& context)
    {
        CreateRHIColorBuffer(context);
        CreateRHIDepthBuffer(context);
        CreateRHIShadowMap(context);
        CreateRHIShadowMask(context);
    }

    ResourceManager::~ResourceManager()
    {
    }

    void ResourceManager::Cleanup(const Context::Context& context)
    {
        auto& rhiDevice = context.GetVulkanContext().GetRHIDevice();
        rhiDevice.WaitIdle();
        m_RHIColorBuffer.reset();
        m_RHIDepthBuffer.reset();
        m_RHIShadowMap.reset();
        m_RHIShadowMask.reset();
    }

    void ResourceManager::ResizeFrameBufferSizeDependenteResources(const Context::Context& context)
    {
        auto& rhiDevice = context.GetVulkanContext().GetRHIDevice();
        rhiDevice.WaitIdle();
        m_RHIColorBuffer.reset();
        m_RHIDepthBuffer.reset();
        m_RHIShadowMask.reset();

        CreateRHIColorBuffer(context);
        CreateRHIDepthBuffer(context);
        CreateRHIShadowMask(context);
    }

    void ResourceManager::ResizeSettingsDependenteResources(const Context::Context& context)
    {
        auto& settings = context.GetEngineContext().GetSettings();
        auto& shadowmapSettings = settings.GetShadowmapSettings();

        if (shadowmapSettings->IsDirty())
        {
            context.GetVulkanContext().GetRHIDevice().WaitIdle();
            m_RHIShadowMap.reset();
            CreateRHIShadowMap(context);

            shadowmapSettings->CleanDirtyState();
        }
    }

    void ResourceManager::CreateRHIColorBuffer(const Context::Context& context)
    {
        auto& vulkanContext = context.GetVulkanContext();
        auto& swapchain     = vulkanContext.GetRHISwapchain();

        RHI::TextureDesc desc;
        desc.m_Width  = swapchain.GetWidth();
        desc.m_Height = swapchain.GetHeight();
        desc.m_Format = RHI::Format::R16G16B16A16Unorm;
        desc.m_Usage  = RHI::TextureUsage::ColorAttachment
                      | RHI::TextureUsage::TransferSrc
                      | RHI::TextureUsage::TransferDst
                      | RHI::TextureUsage::Storage;

        m_RHIColorBuffer = vulkanContext.GetRHIDevice().CreateTexture(desc);
        LOGINFO("RHI color buffer created");
    }

    void ResourceManager::CreateRHIDepthBuffer(const Context::Context& context)
    {
        auto& vulkanContext = context.GetVulkanContext();
        auto& swapchain     = vulkanContext.GetRHISwapchain();

        RHI::TextureDesc desc;
        desc.m_Width  = swapchain.GetWidth();
        desc.m_Height = swapchain.GetHeight();
        desc.m_Format = vulkanContext.GetRHIDevice().GetPreferredDepthFormat();
        desc.m_Usage  = RHI::TextureUsage::DepthStencil;

        m_RHIDepthBuffer = vulkanContext.GetRHIDevice().CreateTexture(desc);
        LOGINFO("RHI depth buffer created");
    }

    void ResourceManager::CreateRHIShadowMap(const Context::Context& context)
    {
        auto& vulkanContext    = context.GetVulkanContext();
        auto& settings         = context.GetEngineContext().GetSettings();
        auto& shadowmapSettings = settings.GetShadowmapSettings();
        auto shadowmapSize     = shadowmapSettings->GetShadowmapSize();

        RHI::TextureDesc desc;
        desc.m_Width  = shadowmapSize;
        desc.m_Height = shadowmapSize;
        desc.m_Format = vulkanContext.GetRHIDevice().GetPreferredDepthFormat();
        desc.m_Usage  = RHI::TextureUsage::DepthStencil;

        m_RHIShadowMap = vulkanContext.GetRHIDevice().CreateTexture(desc);
        LOGINFO("RHI shadow map created");
    }

    void ResourceManager::CreateRHIShadowMask(const Context::Context& context)
    {
        auto& vulkanContext = context.GetVulkanContext();
        auto& swapchain     = vulkanContext.GetRHISwapchain();

        RHI::TextureDesc desc;
        desc.m_Width  = swapchain.GetWidth();
        desc.m_Height = swapchain.GetHeight();
        desc.m_Format = RHI::Format::R16Float;
        desc.m_Usage  = RHI::TextureUsage::Sampled | RHI::TextureUsage::Storage;

        m_RHIShadowMask = vulkanContext.GetRHIDevice().CreateTexture(desc);
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

}
