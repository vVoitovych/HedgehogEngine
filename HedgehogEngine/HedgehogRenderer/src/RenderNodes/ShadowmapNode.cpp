#include "ShadowmapNode.hpp"

#include "RenderGraph/RenderContext.hpp"
#include "RenderPasses/ShadowmapPass/ShadowmapPass.hpp"

namespace Renderer
{
    ShadowmapNode::ShadowmapNode(RHI::IRHIDevice& device,
                                 const HedgehogSettings::Settings& settings,
                                 const ResourceManager& resourceManager,
                                 ShaderManager& shaderManager)
        : m_Pass(std::make_unique<ShadowmapPass>(device, settings, resourceManager, shaderManager))
    {}

    void ShadowmapNode::Execute(RenderContext& ctx)
    {
        m_Pass->Render(ctx.GetFrameData(), ctx.GetResourceManager(),
                       ctx.GetCommandList(), ctx.GetFrameIndex());
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
