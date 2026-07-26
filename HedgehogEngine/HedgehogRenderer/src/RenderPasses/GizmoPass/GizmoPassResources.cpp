#include "GizmoPassResources.hpp"

#include "RenderPasses/PassInitContext.hpp"

#include "Pipeline/ShaderLoader.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHIRenderPass.hpp"
#include "RHI/api/IRHIPipeline.hpp"

namespace Renderer
{
    GizmoPassResources::GizmoPassResources(const PassInitContext& init)
    {
        const auto sd = ShaderLoader::Load(init.Device,
            "engine://HedgehogEngine/HedgehogRenderer/assets/Shaders/Gizmo.shader",
            init.FileSystem);

        // Colour-only render pass that loads (preserves) the forward result and draws lines over
        // it. Format matches ResourceManager's scene/game colour buffers — the same renderer-wide
        // convention ForwardPassResources uses (see PassInitContext).
        RHI::RenderPassDesc rpDesc;
        rpDesc.ColorAttachments.push_back(RHI::AttachmentDesc{
            RHI::Format::R16G16B16A16Unorm,
            RHI::LoadOp::Load,
            RHI::StoreOp::Store,
            RHI::LoadOp::DontCare,
            RHI::StoreOp::DontCare,
            RHI::ImageLayout::ColorAttachment,
            RHI::ImageLayout::ColorAttachment
        });
        m_RenderPass = init.Device.CreateRenderPass(rpDesc);

        auto pipelineDesc       = sd.Pipeline;   // no descriptor sets: view-proj is a push constant
        pipelineDesc.RenderPass = m_RenderPass.get();
        m_Pipeline = init.Device.CreateGraphicsPipeline(pipelineDesc);
    }

    GizmoPassResources::~GizmoPassResources()
    {
    }
}
