#include "HedgehogRenderer/Renderer.hpp"

#include "HedgehogEngine/api/HedgehogEngine.hpp"
#include "HedgehogEngine/api/WindowContext.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"

#include "RHIContext/RHIContext.hpp"
#include "ThreadContext/ThreadContext.hpp"
#include "RenderGraph/RenderGraph.hpp"
#include "RenderGraph/RenderContext.hpp"
#include "ResourceManager/ResourceManager.hpp"

#include "RenderNodes/InitNode.hpp"
#include "RenderNodes/ShadowmapNode.hpp"
#include "RenderNodes/DepthPrepassNode.hpp"
#include "RenderNodes/ForwardNode.hpp"
#include "RenderNodes/GuiNode.hpp"
#include "RenderNodes/PresentNode.hpp"

#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"
#include "HedgehogSettings/Settings/HedgehogSettings.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHISwapchain.hpp"

#include "Logger/api/Logger.hpp"

namespace Renderer
{
    Renderer::Renderer(HedgehogEngine::HedgehogEngine& context)
    {
        auto& windowContext = context.GetWindowContext();
        auto& engineContext = context.GetEngineContext();

        m_RHIContext      = std::make_unique<RHIContext>(windowContext);
        m_ThreadContext   = std::make_unique<ThreadContext>(m_RHIContext->GetRHIDevice());
        m_ResourceManager = std::make_unique<ResourceManager>(
            m_RHIContext->GetRHIDevice(),
            m_RHIContext->GetRHISwapchain(),
            engineContext.GetSettings());

        auto& device   = m_RHIContext->GetRHIDevice();
        auto& window   = windowContext.GetWindow();
        auto& settings = engineContext.GetSettings();

        m_RenderGraph = std::make_unique<RenderGraph>();
        m_RenderGraph->AddNode(std::make_unique<InitNode>());
        m_RenderGraph->AddNode(std::make_unique<ShadowmapNode>(device, settings, *m_ResourceManager));
        m_RenderGraph->AddNode(std::make_unique<DepthPrepassNode>(device, *m_ResourceManager));
        m_RenderGraph->AddNode(std::make_unique<ForwardNode>(device, *m_ResourceManager));
        m_RenderGraph->AddNode(std::make_unique<GuiNode>(window, device, *m_ResourceManager));
        m_RenderGraph->AddNode(std::make_unique<PresentNode>());
        m_RenderGraph->Compile();
    }

    Renderer::~Renderer()
    {
    }

    void Renderer::Cleanup(HedgehogEngine::HedgehogEngine& context)
    {
        auto& device = m_RHIContext->GetRHIDevice();
        device.WaitIdle();
        m_RenderGraph->Cleanup(device);
        m_ResourceManager->Cleanup(device);
        m_ThreadContext->Cleanup(device);
        m_RHIContext->Cleanup();
    }

    void Renderer::BeginGui()
    {
        m_RenderGraph->BeginFrame();
    }

    void* Renderer::GetSceneViewTextureId() const
    {
        return m_RenderGraph->GetSceneViewTextureId();
    }

    float Renderer::GetAspectRatio() const
    {
        const auto& scene = m_ResourceManager->GetSceneColorBuffer();
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

        m_ResourceManager->SyncResources(device, engineContext);

        const auto&    frameData  = engineContext.GetFrameData();
        const uint32_t frameIndex = m_ThreadContext->GetFrameIndex();

        m_RenderGraph->UpdateData(frameData, frameIndex, engineContext.GetSettings());

        if (windowContext.IsWindowResized() || window.IsResized())
        {
            m_RenderGraph->DiscardFrame();

            if (window.IsResized())
                window.ResetResizedFlag();
            windowContext.ResetWindowResizeState();

            device.WaitIdle();
            m_RHIContext->RecreateSwapchain(windowContext);

            m_ResourceManager->ResizeFrameBufferSizeDependenteResources(device, swapchain);
            m_RenderGraph->OnWindowResize(device, *m_ResourceManager);

            return;
        }

        if (engineContext.GetSettings().IsDirty())
        {
            m_ResourceManager->ResizeSettingsDependenteResources(device, engineContext.GetSettings());
            m_RenderGraph->OnSettingsChanged(device, engineContext.GetSettings(), *m_ResourceManager);
            engineContext.GetSettings().CleanDirtyState();
        }

        RenderContext ctx(
            frameData,
            device,
            swapchain,
            m_ThreadContext->GetCommandList(),
            m_ThreadContext->GetFence(),
            m_ThreadContext->GetImageAvailableSemaphore(),
            m_ThreadContext->GetRenderFinishedSemaphore(),
            frameIndex,
            *m_ResourceManager);

        m_RenderGraph->Execute(ctx);

        m_ThreadContext->NextFrame();

        // Apply pending scene view resize after frame submission so the ImGui draw
        // data referencing the old descriptor has already been consumed by the GPU.
        if (m_DesiredSceneW > 0 && m_DesiredSceneH > 0)
        {
            const auto& sceneBuffer = m_ResourceManager->GetSceneColorBuffer();
            if (sceneBuffer.GetWidth()  != m_DesiredSceneW ||
                sceneBuffer.GetHeight() != m_DesiredSceneH)
            {
                device.WaitIdle();
                m_ResourceManager->ResizeSceneView(device, m_DesiredSceneW, m_DesiredSceneH);
                m_RenderGraph->OnSceneViewResize(device, *m_ResourceManager);
            }
        }
    }
}
