#include "HedgehogRenderer/Renderer.hpp"

#include "Profiling/Profiler.hpp"
#include "RHIContext/RHIContext.hpp"
#include "ThreadContext/ThreadContext.hpp"
#include "RenderGraph/RenderGraph.hpp"
#include "RenderGraph/RenderGraphTypes.hpp"
#include "RenderQueue/RenderQueue.hpp"
#include "ResourceRegistry/ResourceRegistry.hpp"

#include "HedgehogWindow/api/Window.hpp"

#include "HedgehogCommon/api/Frame/FrameData.hpp"
#include "HedgehogCommon/api/Resource/IResourceCatalog.hpp"

#include "HedgehogSettings/api/HedgehogSettings.hpp"
#include "HedgehogSettings/api/ShadowmapingSettings.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHISwapchain.hpp"
#include "RHI/api/IRHITexture.hpp"
#include "RHI/api/RHIDiagnostics.hpp"

#include "Logger/api/Logger.hpp"

namespace Renderer
{
    bool AreValidationLayersEnabled()
    {
        return RHI::AreValidationLayersEnabled();
    }

    uint32_t GetValidationErrorCount()
    {
        return RHI::GetValidationErrorCount();
    }

    uint32_t GetValidationWarningCount()
    {
        return RHI::GetValidationWarningCount();
    }

    namespace
    {
        // Declares the graph's four transients with the same descs ResourceManager used to own.
        void DeclareGraphTransients(RenderGraph& graph, RHI::IRHIDevice& device,
                                    const HedgehogSettings::Settings& settings)
        {
            GraphTextureDesc guiColorDesc;
            guiColorDesc.m_SizeClass = SizeClass::SwapchainRelative;
            guiColorDesc.m_Format    = RHI::Format::R16G16B16A16Unorm;
            guiColorDesc.m_Usage     = RHI::TextureUsage::ColorAttachment
                                      | RHI::TextureUsage::TransferSrc
                                      | RHI::TextureUsage::TransferDst
                                      | RHI::TextureUsage::Storage;
            graph.DeclareTexture(GraphResourceNames::GUI_COLOR, guiColorDesc);

            GraphTextureDesc sceneDepthDesc;
            sceneDepthDesc.m_SizeClass = SizeClass::SceneViewRelative;
            sceneDepthDesc.m_Format    = device.GetPreferredDepthFormat();
            sceneDepthDesc.m_Usage     = RHI::TextureUsage::DepthStencil;
            graph.DeclareTexture(GraphResourceNames::SCENE_DEPTH, sceneDepthDesc);

            GraphTextureDesc sceneColorDesc;
            sceneColorDesc.m_SizeClass = SizeClass::SceneViewRelative;
            sceneColorDesc.m_Format    = RHI::Format::R16G16B16A16Unorm;
            sceneColorDesc.m_Usage     = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled;
            graph.DeclareTexture(GraphResourceNames::SCENE_COLOR, sceneColorDesc);

            const uint32_t shadowmapSize = settings.GetShadowmapSettings()->GetShadowmapSize();
            GraphTextureDesc shadowDepthDesc;
            shadowDepthDesc.m_SizeClass   = SizeClass::Fixed;
            shadowDepthDesc.m_Format      = device.GetPreferredDepthFormat();
            shadowDepthDesc.m_Usage       = RHI::TextureUsage::DepthStencil;
            shadowDepthDesc.m_FixedWidth  = shadowmapSize;
            shadowDepthDesc.m_FixedHeight = shadowmapSize;
            graph.DeclareTexture(GraphResourceNames::SHADOW_DEPTH, shadowDepthDesc);
        }
    }

    Renderer::Renderer(HW::Window& window,
                       const HedgehogSettings::Settings& settings,
                       const FS::FileSystemManager& fileSystem)
        : m_Window(window)
    {
        m_RHIContext    = std::make_unique<RHIContext>(window);
        m_ThreadContext = std::make_unique<ThreadContext>(m_RHIContext->GetRHIDevice());

        auto& device    = m_RHIContext->GetRHIDevice();
        auto& swapchain = m_RHIContext->GetRHISwapchain();

        m_ResourceRegistry = std::make_unique<HR::ResourceRegistry>(device);

        m_Graph = std::make_unique<RenderGraph>();
        DeclareGraphTransients(*m_Graph, device, settings);
        // No passes are registered yet — Compile() just allocates the four declared transients.
        // Scene-view starts out swapchain-sized; SetSceneViewSize() narrows it later.
        m_Graph->Compile(device, swapchain.GetWidth(), swapchain.GetHeight(),
                          swapchain.GetWidth(), swapchain.GetHeight());
        LOGINFO("Render graph transients created");

        m_RenderQueue = std::make_unique<RenderQueue>(
            device,
            window,
            settings,
            *m_Graph,
            *m_ResourceRegistry,
            fileSystem);
    }

    Renderer::~Renderer()
    {
    }

    void Renderer::Cleanup()
    {
        auto& device = m_RHIContext->GetRHIDevice();
        device.WaitIdle();
        m_RenderQueue->Cleanup(device);
        m_Graph->Cleanup(device);
        m_ResourceRegistry->Cleanup(device);
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
        const auto& scene = m_Graph->GetTexture(GraphResourceNames::SCENE_COLOR);
        return static_cast<float>(scene.GetWidth()) / static_cast<float>(scene.GetHeight());
    }

    void Renderer::SetSceneViewSize(uint32_t width, uint32_t height)
    {
        m_DesiredSceneW = width;
        m_DesiredSceneH = height;
    }

    void Renderer::BeginFrameStatsCapture()
    {
        m_RenderQueue->GetFrameStats().BeginCapture();
    }

    void Renderer::EndFrameStatsCaptureAndLogReport()
    {
        auto& stats = m_RenderQueue->GetFrameStats();
        stats.EndCapture();
        stats.LogReport();
    }

    void Renderer::DrawFrame(const HedgehogEngine::FrameData& frameData,
                             HedgehogEngine::IResourceCatalog& catalog,
                             HedgehogSettings::Settings&       settings)
    {
        HH_PROFILE_ZONE("DrawFrame");
        ScopedCpuSample sample(m_RenderQueue->GetFrameStats(), "DrawFrame(total)");

        auto& device    = m_RHIContext->GetRHIDevice();
        auto& swapchain = m_RHIContext->GetRHISwapchain();

        m_ResourceRegistry->SyncMeshes(catalog, device);
        m_ResourceRegistry->SyncMaterials(catalog, device);

        const uint32_t frameIndex = m_ThreadContext->GetFrameIndex();

        if (m_Window.IsResized())
        {
            m_RenderQueue->DiscardGui();

            m_Window.ResetResizedFlag();

            device.WaitIdle();
            m_RHIContext->RecreateSwapchain(m_Window);

            m_Graph->SetSwapchainSize(swapchain.GetWidth(), swapchain.GetHeight());
            m_Graph->Invalidate(SizeClass::SwapchainRelative, device);

            return;
        }

        if (settings.IsDirty())
        {
            auto& shadowmapSettings = settings.GetShadowmapSettings();
            if (shadowmapSettings->IsDirty())
            {
                const uint32_t shadowmapSize = shadowmapSettings->GetShadowmapSize();
                m_Graph->SetFixedSize(GraphResourceNames::SHADOW_DEPTH, shadowmapSize, shadowmapSize);
                m_Graph->Invalidate(SizeClass::Fixed, device);

                shadowmapSettings->CleanDirtyState();
            }

            settings.CleanDirtyState();
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
            settings,
            *m_ResourceRegistry);

        m_ThreadContext->NextFrame();

        // Apply pending scene view resize at end of frame so the current frame's
        // ImGui draw data (which references the old descriptor) has already been submitted.
        if (m_DesiredSceneW > 0 && m_DesiredSceneH > 0)
        {
            const auto& sceneBuffer = m_Graph->GetTexture(GraphResourceNames::SCENE_COLOR);
            if (sceneBuffer.GetWidth() != m_DesiredSceneW || sceneBuffer.GetHeight() != m_DesiredSceneH)
            {
                device.WaitIdle();
                m_Graph->SetSceneViewSize(m_DesiredSceneW, m_DesiredSceneH);
                m_Graph->Invalidate(SizeClass::SceneViewRelative, device);
            }
        }

        HH_PROFILE_FRAME();
    }
}
