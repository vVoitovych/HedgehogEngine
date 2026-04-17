#include "ResourceManager.hpp"

#include "HedgehogContext/Context/Context.hpp"
#include "HedgehogContext/Context/VulkanContext.hpp"
#include "HedgehogContext/Context/EngineContext.hpp"
#include "HedgehogSettings/Settings/HedgehogSettings.hpp"
#include "HedgehogSettings/Settings/ShadowmapingSettings.hpp"

#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"
#include "HedgehogWrappers/Wrappeers/SwapChain/SwapChain.hpp"
#include "HedgehogWrappers/Wrappeers/Resources/Image/Image.hpp"

#include "RHI/api/IRHIDevice.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"

#include "Logger/api/Logger.hpp"

namespace Renderer
{
    ResourceManager::ResourceManager(const Context::Context& context)
    {
        CreateColorBuffer(context);
        CreateDepthBuffer(context);
        CreateShadowMap(context);
        CreateShadowMask(context);

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
        auto& vulkanContext = context.GetVulkanContext();
        m_ColorBuffer->Cleanup(vulkanContext.GetDevice());
        m_DepthBuffer->Cleanup(vulkanContext.GetDevice());
        m_ShadowMap->Cleanup(vulkanContext.GetDevice());
        m_ShadowMask->Cleanup(vulkanContext.GetDevice());

        // RHI textures self-destruct via unique_ptr; wait for idle first.
        vulkanContext.GetRHIDevice().WaitIdle();
        m_RHIColorBuffer.reset();
        m_RHIDepthBuffer.reset();
        m_RHIShadowMap.reset();
        m_RHIShadowMask.reset();
    }

    void ResourceManager::ResizeFrameBufferSizeDependenteResources(const Context::Context& context)
    {
        auto& vulkanContext = context.GetVulkanContext();
        m_ColorBuffer->Cleanup(vulkanContext.GetDevice());
        m_DepthBuffer->Cleanup(vulkanContext.GetDevice());
        m_ShadowMask->Cleanup(vulkanContext.GetDevice());

        CreateColorBuffer(context);
        CreateDepthBuffer(context);
        CreateShadowMask(context);

        vulkanContext.GetRHIDevice().WaitIdle();
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
            m_ShadowMap->Cleanup(context.GetVulkanContext().GetDevice());
            CreateShadowMap(context);

            context.GetVulkanContext().GetRHIDevice().WaitIdle();
            m_RHIShadowMap.reset();
            CreateRHIShadowMap(context);

            shadowmapSettings->CleanDirtyState();
        }
    }

    const Wrappers::Image& ResourceManager::GetColorBuffer() const
    {
        return *m_ColorBuffer;
    }

    const Wrappers::Image& ResourceManager::GetDepthBuffer() const
    {
        return *m_DepthBuffer;
    }

    const Wrappers::Image& ResourceManager::GetShadowMap() const
    {
        return *m_ShadowMap;
    }

    const Wrappers::Image& ResourceManager::GetShadowMask() const
    {
        return *m_ShadowMask;
    }

    void ResourceManager::CreateDepthBuffer(const Context::Context& context)
    {
        auto& vulkanContext = context.GetVulkanContext();
        auto depthFormat = vulkanContext.GetDevice().FindDepthFormat();
        auto extend = vulkanContext.GetSwapChain().GetSwapChainExtent();

        m_DepthBuffer = std::make_unique<Wrappers::Image>(
            vulkanContext.GetDevice(),
            extend.width,
            extend.height,
            depthFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        m_DepthBuffer->CreateImageView(vulkanContext.GetDevice(), depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
        vulkanContext.GetDevice().SetObjectName(reinterpret_cast<uint64_t>(m_DepthBuffer->GetNativeImage()), VK_OBJECT_TYPE_IMAGE, "DepthBuffer");
        LOGINFO("Depth buffer created");
    }

    void ResourceManager::CreateColorBuffer(const Context::Context& context)
    {
        auto& vulkanContext = context.GetVulkanContext();
        auto colorFormat = VK_FORMAT_R16G16B16A16_UNORM;
        auto extend = vulkanContext.GetSwapChain().GetSwapChainExtent();

        m_ColorBuffer = std::make_unique<Wrappers::Image>(
            vulkanContext.GetDevice(),
            extend.width,
            extend.height,
            colorFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        m_ColorBuffer->CreateImageView(vulkanContext.GetDevice(), colorFormat, VK_IMAGE_ASPECT_COLOR_BIT);

        vulkanContext.GetDevice().SetObjectName(reinterpret_cast<uint64_t>(m_ColorBuffer->GetNativeImage()), VK_OBJECT_TYPE_IMAGE, "ColorBuffer");
        LOGINFO("Color buffer created");
    }

    void ResourceManager::CreateShadowMap(const Context::Context& context)
    {
        auto& vulkanContext = context.GetVulkanContext();
        auto depthFormat = vulkanContext.GetDevice().FindDepthFormat();
        auto& settings = context.GetEngineContext().GetSettings();
        auto& shadowmapSettings = settings.GetShadowmapSettings();
        auto shadowmapSize = shadowmapSettings->GetShadowmapSize();

        m_ShadowMap = std::make_unique<Wrappers::Image>(
            vulkanContext.GetDevice(),
            shadowmapSize,
            shadowmapSize,
            depthFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        m_ShadowMap->CreateImageView(vulkanContext.GetDevice(), depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
        vulkanContext.GetDevice().SetObjectName(reinterpret_cast<uint64_t>(m_ShadowMap->GetNativeImage()), VK_OBJECT_TYPE_IMAGE, "ShadowMap");
        LOGINFO("Shadow map created");
    }

    void ResourceManager::CreateShadowMask(const Context::Context& context)
    {
        auto& vulkanContext = context.GetVulkanContext();
        auto colorFormat = VK_FORMAT_R16_SFLOAT;
        auto extend = vulkanContext.GetSwapChain().GetSwapChainExtent();

        m_ShadowMask = std::make_unique<Wrappers::Image>(
            vulkanContext.GetDevice(),
            extend.width,
            extend.height,
            colorFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        m_ShadowMask->CreateImageView(vulkanContext.GetDevice(), colorFormat, VK_IMAGE_ASPECT_COLOR_BIT);

        vulkanContext.GetDevice().SetObjectName(reinterpret_cast<uint64_t>(m_ShadowMask->GetNativeImage()), VK_OBJECT_TYPE_IMAGE, "ShadowMask");
        LOGINFO("Shadow mask created");
    }

    // ── RHI creation helpers ──────────────────────────────────────────────────

    void ResourceManager::CreateRHIColorBuffer(const Context::Context& context)
    {
        auto& vulkanContext = context.GetVulkanContext();
        auto extend = vulkanContext.GetSwapChain().GetSwapChainExtent();

        RHI::TextureDesc desc;
        desc.m_Width  = extend.width;
        desc.m_Height = extend.height;
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
        auto extend = vulkanContext.GetSwapChain().GetSwapChainExtent();

        RHI::TextureDesc desc;
        desc.m_Width  = extend.width;
        desc.m_Height = extend.height;
        desc.m_Format = vulkanContext.GetRHIDevice().GetPreferredDepthFormat();
        desc.m_Usage  = RHI::TextureUsage::DepthStencil;

        m_RHIDepthBuffer = vulkanContext.GetRHIDevice().CreateTexture(desc);
        LOGINFO("RHI depth buffer created");
    }

    void ResourceManager::CreateRHIShadowMap(const Context::Context& context)
    {
        auto& vulkanContext = context.GetVulkanContext();
        auto& settings = context.GetEngineContext().GetSettings();
        auto& shadowmapSettings = settings.GetShadowmapSettings();
        auto shadowmapSize = shadowmapSettings->GetShadowmapSize();

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
        auto extend = vulkanContext.GetSwapChain().GetSwapChainExtent();

        RHI::TextureDesc desc;
        desc.m_Width  = extend.width;
        desc.m_Height = extend.height;
        desc.m_Format = RHI::Format::R16Float;
        desc.m_Usage  = RHI::TextureUsage::Sampled | RHI::TextureUsage::Storage;

        m_RHIShadowMask = vulkanContext.GetRHIDevice().CreateTexture(desc);
        LOGINFO("RHI shadow mask created");
    }

    // ── RHI accessors ────────────────────────────────────────────────────────

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

