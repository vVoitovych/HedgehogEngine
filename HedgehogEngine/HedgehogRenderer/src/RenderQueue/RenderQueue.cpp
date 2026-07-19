#include "RenderQueue.hpp"

#include "RenderGraph/RenderGraph.hpp"
#include "RenderGraph/RenderGraphTypes.hpp"
#include "RenderPasses/InitPass/InitPass.hpp"
#include "RenderPasses/DepthPrepass/DepthPrePass.hpp"
#include "RenderPasses/ShadowmapPass/ShadowmapPass.hpp"
#include "RenderPasses/ForwardPass/ForwardPass.hpp"
#include "RenderPasses/PresentPass/PresentPass.hpp"
#include "RenderPasses/GuiPass/GuiPass.hpp"

#include "HedgehogSettings/api/HedgehogSettings.hpp"

#include "HedgehogCommon/api/Frame/FrameData.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHISwapchain.hpp"
#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHISyncPrimitive.hpp"

namespace Renderer
{
    RenderQueue::RenderQueue(RHI::IRHIDevice&                  device,
                             HW::Window&                       window,
                             const HedgehogSettings::Settings& settings,
                             RenderGraph&                      graph,
                             HR::ResourceRegistry&             resourceRegistry,
                             const FS::FileSystemManager&      fileSystem)
    {
        m_Graph = &graph;

        m_InitPass = std::make_unique<InitPass>();
        m_Graph->AddAndCompilePass(m_InitPass.get(), device);

        m_ShadowmapPass = std::make_unique<ShadowmapPass>(device, settings, fileSystem);
        m_Graph->AddAndCompilePass(m_ShadowmapPass.get(), device);

        m_DepthPrePass = std::make_unique<DepthPrePass>(device, fileSystem);
        m_Graph->AddAndCompilePass(m_DepthPrePass.get(), device);

        m_ForwardPass = std::make_unique<ForwardPass>(device, resourceRegistry, fileSystem);
        m_Graph->AddAndCompilePass(m_ForwardPass.get(), device);

        m_GuiPass = std::make_unique<GuiPass>(window, device);
        m_Graph->AddAndCompilePass(m_GuiPass.get(), device);

        m_PresentPass = std::make_unique<PresentPass>();
        m_Graph->AddAndCompilePass(m_PresentPass.get(), device);
    }

    RenderQueue::~RenderQueue()
    {
    }

    void RenderQueue::Cleanup(RHI::IRHIDevice& device)
    {
        m_InitPass->Cleanup(device);
        m_DepthPrePass->Cleanup(device);
        m_ShadowmapPass->Cleanup(device);
        m_ForwardPass->Cleanup(device);
        m_GuiPass->Cleanup(device);
        m_PresentPass->Cleanup(device);
    }

    void RenderQueue::BeginGui()
    {
        m_GuiPass->BeginFrame();
    }

    void RenderQueue::DiscardGui()
    {
        m_GuiPass->DiscardFrame();
    }

    void RenderQueue::Render(const HedgehogEngine::FrameData& frame,
                             RHI::IRHIDevice&     device,
                             RHI::IRHISwapchain&  swapchain,
                             RHI::IRHICommandList& cmd,
                             RHI::IRHIFence&      fence,
                             RHI::IRHISemaphore&  imageAvailableSemaphore,
                             RHI::IRHISemaphore&  renderFinishedSemaphore,
                             uint32_t             frameIndex,
                             const HedgehogSettings::Settings& settings,
                             HR::ResourceRegistry& resourceRegistry)
    {
        RenderGraphContext ctx;
        ctx.m_FrameData                = &frame;
        ctx.m_FrameIndex                = frameIndex;
        ctx.m_Device                    = &device;
        ctx.m_CommandList                = &cmd;
        ctx.m_Swapchain                  = &swapchain;
        ctx.m_Fence                      = &fence;
        ctx.m_ImageAvailableSemaphore    = &imageAvailableSemaphore;
        ctx.m_RenderFinishedSemaphore    = &renderFinishedSemaphore;
        ctx.m_ResourceRegistry           = &resourceRegistry;
        ctx.m_Settings                   = &settings;

        m_Graph->Update(ctx);
        m_Graph->Execute(ctx, m_FrameStats);
    }

    void* RenderQueue::GetSceneViewTextureId() const
    {
        return m_GuiPass->GetSceneViewTextureId();
    }
}
