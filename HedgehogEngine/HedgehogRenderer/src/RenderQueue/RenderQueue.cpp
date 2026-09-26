#include "RenderQueue.hpp"

#include "Profiling/Profiler.hpp"
#include "ResourceManager/ResourceManager.hpp"
#include "RenderPasses/DepthPrepass/DepthPrePass.hpp"
#include "RenderPasses/ShadowmapPass/ShadowmapPass.hpp"
#include "RenderPasses/ForwardPass/ForwardPass.hpp"

#include "HedgehogSettings/api/HedgehogSettings.hpp"

#include "HedgehogCommon/api/Frame/FrameData.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHISwapchain.hpp"
#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHISyncPrimitive.hpp"
#include "RHI/api/IRHITexture.hpp"

namespace Renderer
{
    RenderQueue::RenderQueue(RHI::IRHIDevice&                  device,
                             const HedgehogSettings::Settings& settings,
                             ResourceManager&                  resourceManager,
                             const FS::FileSystemManager&      fileSystem)
    {
        m_DepthPrePass  = std::make_unique<DepthPrePass>(device, resourceManager, fileSystem);
        m_ShadowmapPass = std::make_unique<ShadowmapPass>(device, settings, resourceManager, fileSystem);
        m_ForwardPass   = std::make_unique<ForwardPass>(device, resourceManager, fileSystem);
    }

    RenderQueue::~RenderQueue()
    {
    }

    void RenderQueue::Cleanup(RHI::IRHIDevice& device)
    {
        m_DepthPrePass->Cleanup(device);
        m_ShadowmapPass->Cleanup(device);
        m_ForwardPass->Cleanup(device);
    }

    void RenderQueue::Render(const HedgehogEngine::FrameData& frame,
                             RHI::IRHICommandList&            cmd,
                             uint32_t                         frameIndex,
                             const ResourceManager&           resourceManager)
    {
        {
            HH_PROFILE_ZONE("ShadowmapPass");
            ScopedCpuSample sample(m_FrameStats, "ShadowmapPass");
            m_ShadowmapPass->Render(frame, resourceManager, cmd, frameIndex);
        }

        {
            HH_PROFILE_ZONE("DepthPrePass");
            ScopedCpuSample sample(m_FrameStats, "DepthPrePass");
            m_DepthPrePass->Render(frame, resourceManager, cmd, frameIndex);
        }

        {
            HH_PROFILE_ZONE("ForwardPass");
            ScopedCpuSample sample(m_FrameStats, "ForwardPass");
            m_ForwardPass->Render(frame, resourceManager, cmd, frameIndex);
        }
    }

    void RenderQueue::UpdateData(const HedgehogEngine::FrameData&             frame,
                                 uint32_t                          frameIndex,
                                 const HedgehogSettings::Settings& settings)
    {
        m_ShadowmapPass->UpdateData(frame, frameIndex, settings);
    }

    void RenderQueue::ResizeSceneView(RHI::IRHIDevice& device, const ResourceManager& resourceManager)
    {
        m_DepthPrePass->ResizeResources(device, resourceManager);
        m_ForwardPass->ResizeResources(device, resourceManager);
    }

    void RenderQueue::UpdateResources(RHI::IRHIDevice&                  device,
                                      const HedgehogSettings::Settings&  settings,
                                      const ResourceManager&             resourceManager)
    {
        m_ShadowmapPass->UpdateResources(device, settings, resourceManager);
    }
}
