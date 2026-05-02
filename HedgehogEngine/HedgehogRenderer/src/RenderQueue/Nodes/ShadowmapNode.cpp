#include "ShadowmapNode.hpp"

#include "../RenderContext.hpp"
#include "../../RenderPasses/ShadowmapPass/ShadowmapPass.hpp"

namespace Renderer
{
    ShadowmapNode::ShadowmapNode(RHI::IRHIDevice& device,
                                  const HedgehogSettings::Settings& settings,
                                  const ResourceManager& resourceManager)
        : m_Pass(std::make_unique<ShadowmapPass>(device, settings, resourceManager))
    {}

    ShadowmapNode::~ShadowmapNode() = default;

    void ShadowmapNode::Render(RenderContext& ctx)
    {
        m_Pass->Render(*ctx.m_FrameData, *ctx.m_ResourceManager, *ctx.m_Cmd, ctx.m_FrameIndex);
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

    void ShadowmapNode::OnUpdateResources(RHI::IRHIDevice& device,
                                           const HedgehogSettings::Settings& settings,
                                           const ResourceManager& rm)
    {
        m_Pass->UpdateResources(device, settings, rm);
    }

} // namespace Renderer
