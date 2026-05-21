#include "ShadowmapNode.hpp"

#include "RenderPasses/ShadowmapPass/ShadowmapPass.hpp"

namespace Renderer
{
    ShadowmapNode::ShadowmapNode(RHI::IRHIDevice& device,
                                 const HedgehogSettings::Settings& settings,
                                 const ResourceManager& resourceManager)
        : m_Pass(std::make_unique<ShadowmapPass>(device, settings, resourceManager))
    {}

    void ShadowmapNode::Execute(NodeContext& ctx)
    {
        m_Pass->Render(ctx.frame, ctx.resourceManager, ctx.cmd, ctx.frameIndex);
    }

    void ShadowmapNode::Cleanup(RHI::IRHIDevice& device)
    {
        m_Pass->Cleanup(device);
    }

    void ShadowmapNode::UpdateData(const HedgehogEngine::FrameData& frame,
                                   uint32_t frameIndex,
                                   const HedgehogSettings::Settings& settings)
    {
        m_Pass->UpdateData(frame, frameIndex, settings);
    }

    void ShadowmapNode::OnSettingsChanged(RHI::IRHIDevice& device,
                                          const HedgehogSettings::Settings& settings,
                                          const ResourceManager& resourceManager)
    {
        m_Pass->UpdateResources(device, settings, resourceManager);
    }
}
