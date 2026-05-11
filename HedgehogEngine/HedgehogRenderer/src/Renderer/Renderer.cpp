#include "HedgehogRenderer/Renderer.hpp"

#include "HedgehogEngine/api/HedgehogEngine.hpp"
#include "HedgehogEngine/api/WindowContext.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"

#include "RHIContext/RHIContext.hpp"
#include "ThreadContext/ThreadContext.hpp"
#include "RenderQueue/RenderQueue.hpp"
#include "ResourceManager/ResourceManager.hpp"
#include "ResourceManager/ResourceNames.hpp"
#include "ResourceRegistry/ResourceRegistry.hpp"

#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"

#include "HedgehogSettings/Settings/HedgehogSettings.hpp"
#include "HedgehogSettings/Settings/ShadowmapingSettings.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHISwapchain.hpp"
#include "RHI/api/RHITypes.hpp"

#include "Logger/api/Logger.hpp"

namespace Renderer
{
    namespace
    {
        RHI::TextureDesc MakeRHIColorDesc(const RHI::IRHISwapchain& swapchain)
        {
            RHI::TextureDesc desc;
            desc.m_Width  = swapchain.GetWidth();
            desc.m_Height = swapchain.GetHeight();
            desc.m_Format = RHI::Format::R16G16B16A16Unorm;
            desc.m_Usage  = RHI::TextureUsage::ColorAttachment
                          | RHI::TextureUsage::TransferSrc
                          | RHI::TextureUsage::TransferDst
                          | RHI::TextureUsage::Storage;
            return desc;
        }

        RHI::TextureDesc MakeDepthDesc(const RHI::IRHIDevice& device,
                                       uint32_t width, uint32_t height)
        {
            RHI::TextureDesc desc;
            desc.m_Width  = width;
            desc.m_Height = height;
            desc.m_Format = device.GetPreferredDepthFormat();
            desc.m_Usage  = RHI::TextureUsage::DepthStencil;
            return desc;
        }

        RHI::TextureDesc MakeShadowMapDesc(const RHI::IRHIDevice& device, uint32_t size)
        {
            RHI::TextureDesc desc;
            desc.m_Width  = size;
            desc.m_Height = size;
            desc.m_Format = device.GetPreferredDepthFormat();
            desc.m_Usage  = RHI::TextureUsage::DepthStencil;
            return desc;
        }

        RHI::TextureDesc MakeShadowMaskDesc(const RHI::IRHISwapchain& swapchain)
        {
            RHI::TextureDesc desc;
            desc.m_Width  = swapchain.GetWidth();
            desc.m_Height = swapchain.GetHeight();
            desc.m_Format = RHI::Format::R16Float;
            desc.m_Usage  = RHI::TextureUsage::Sampled | RHI::TextureUsage::Storage;
            return desc;
        }

        RHI::TextureDesc MakeSceneColorDesc(uint32_t width, uint32_t height)
        {
            RHI::TextureDesc desc;
            desc.m_Width  = width;
            desc.m_Height = height;
            desc.m_Format = RHI::Format::R16G16B16A16Unorm;
            desc.m_Usage  = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled;
            return desc;
        }
    }

    Renderer::Renderer(HedgehogEngine::HedgehogEngine& context)
    {
        auto& windowContext = context.GetWindowContext();
        auto& engineContext = context.GetEngineContext();
        auto& settings      = engineContext.GetSettings();

        m_RHIContext    = std::make_unique<RHIContext>(windowContext);
        m_ThreadContext = std::make_unique<ThreadContext>(m_RHIContext->GetRHIDevice());

        auto& device    = m_RHIContext->GetRHIDevice();
        auto& swapchain = m_RHIContext->GetRHISwapchain();

        m_ResourceRegistry = std::make_unique<HR::ResourceRegistry>(device);

        m_ResourceManager = std::make_unique<ResourceManager>(swapchain);
        m_ResourceManager->CreateTexture(ResourceNames::RHI_COLOR_BUFFER,   device, MakeRHIColorDesc(swapchain));
        m_ResourceManager->CreateTexture(ResourceNames::RHI_DEPTH_BUFFER,   device, MakeDepthDesc(device, swapchain.GetWidth(), swapchain.GetHeight()));
        m_ResourceManager->CreateTexture(ResourceNames::RHI_SHADOW_MAP,     device, MakeShadowMapDesc(device, settings.GetShadowmapSettings()->GetShadowmapSize()));
        m_ResourceManager->CreateTexture(ResourceNames::RHI_SHADOW_MASK,    device, MakeShadowMaskDesc(swapchain));
        m_ResourceManager->CreateTexture(ResourceNames::SCENE_COLOR_BUFFER, device, MakeSceneColorDesc(swapchain.GetWidth(), swapchain.GetHeight()));

        m_RenderQueue = std::make_unique<RenderQueue>(
            device,
            windowContext.GetWindow(),
            settings,
            *m_ResourceManager,
            *m_ResourceRegistry);
    }

    Renderer::~Renderer()
    {
    }

    void Renderer::Cleanup(HedgehogEngine::HedgehogEngine& context)
    {
        auto& device = m_RHIContext->GetRHIDevice();
        device.WaitIdle();
        m_RenderQueue->Cleanup(device);
        m_ResourceRegistry->Cleanup(device);
        m_ResourceManager->Cleanup(device);
        m_ThreadContext->Cleanup(device);
        m_RHIContext->Cleanup();
    }

    void Renderer::BeginGui()
    {
        m_RenderQueue->BeginGui();
    }

    void* Renderer::GetSceneViewTextureId() const
    {
        return m_RenderQueue->GetSceneViewTextureId();
    }

    float Renderer::GetAspectRatio() const
    {
        const auto& scene = m_ResourceManager->GetTexture(ResourceNames::SCENE_COLOR_BUFFER);
        return static_cast<float>(scene.GetWidth()) / static_cast<float>(scene.GetHeight());
    }

    void Renderer::SetSceneViewSize(uint32_t width, uint32_t height)
    {
        m_DesiredSceneW = width;
        m_DesiredSceneH = height;
    }

    void Renderer::DrawFrame(HedgehogEngine::HedgehogEngine& context)
    {
        auto& windowContext = context.GetWindowContext();
        auto& engineContext = context.GetEngineContext();
        auto& window        = windowContext.GetWindow();
        auto& device        = m_RHIContext->GetRHIDevice();
        auto& swapchain     = m_RHIContext->GetRHISwapchain();

        m_ResourceRegistry->SyncMeshes(engineContext.GetMeshContainer(), device);
        m_ResourceRegistry->SyncMaterials(engineContext.GetMaterialContainer(),
                                          engineContext.GetTextureContainer(), device);

        const auto&    frameData  = engineContext.GetFrameData();
        const uint32_t frameIndex = m_ThreadContext->GetFrameIndex();

        m_RenderQueue->UpdateData(frameData, frameIndex, engineContext.GetSettings());

        if (windowContext.IsWindowResized() || window.IsResized())
        {
            m_RenderQueue->DiscardGui();

            if (window.IsResized())
                window.ResetResizedFlag();
            windowContext.ResetWindowResizeState();

            device.WaitIdle();
            m_RHIContext->RecreateSwapchain(windowContext);

            m_ResourceManager->DestroyTexture(ResourceNames::RHI_COLOR_BUFFER);
            m_ResourceManager->DestroyTexture(ResourceNames::RHI_SHADOW_MASK);
            m_ResourceManager->CreateTexture(ResourceNames::RHI_COLOR_BUFFER,  device, MakeRHIColorDesc(swapchain));
            m_ResourceManager->CreateTexture(ResourceNames::RHI_SHADOW_MASK,   device, MakeShadowMaskDesc(swapchain));

            m_RenderQueue->ResizeResources(device, *m_ResourceManager);

            return;
        }

        if (engineContext.GetSettings().IsDirty())
        {
            const auto& shadowmapSettings = *engineContext.GetSettings().GetShadowmapSettings();
            if (shadowmapSettings.IsDirty())
            {
                device.WaitIdle();
                m_ResourceManager->DestroyTexture(ResourceNames::RHI_SHADOW_MAP);
                m_ResourceManager->CreateTexture(ResourceNames::RHI_SHADOW_MAP, device,
                    MakeShadowMapDesc(device, shadowmapSettings.GetShadowmapSize()));
            }
            m_RenderQueue->UpdateResources(device, engineContext.GetSettings(), *m_ResourceManager);
            engineContext.GetSettings().CleanDirtyState();
        }

        m_RenderQueue->Render(
            frameData,
            device,
            swapchain,
            m_ThreadContext->GetCommandList(),
            m_ThreadContext->GetFence(),
            m_ThreadContext->GetImageAvailableSemaphore(),
            m_ThreadContext->GetRenderFinishedSemaphore(),
            frameIndex,
            *m_ResourceManager);

        m_ThreadContext->NextFrame();

        // Apply pending scene view resize at end of frame so the current frame's
        // ImGui draw data (which references the old descriptor) has already been submitted.
        if (m_DesiredSceneW > 0 && m_DesiredSceneH > 0)
        {
            const auto& sceneBuffer = m_ResourceManager->GetTexture(ResourceNames::SCENE_COLOR_BUFFER);
            if (sceneBuffer.GetWidth() != m_DesiredSceneW || sceneBuffer.GetHeight() != m_DesiredSceneH)
            {
                device.WaitIdle();
                m_ResourceManager->DestroyTexture(ResourceNames::RHI_DEPTH_BUFFER);
                m_ResourceManager->DestroyTexture(ResourceNames::SCENE_COLOR_BUFFER);
                m_ResourceManager->CreateTexture(ResourceNames::RHI_DEPTH_BUFFER,   device, MakeDepthDesc(device, m_DesiredSceneW, m_DesiredSceneH));
                m_ResourceManager->CreateTexture(ResourceNames::SCENE_COLOR_BUFFER, device, MakeSceneColorDesc(m_DesiredSceneW, m_DesiredSceneH));
                m_RenderQueue->ResizeSceneView(device, *m_ResourceManager);
            }
        }
    }
}
