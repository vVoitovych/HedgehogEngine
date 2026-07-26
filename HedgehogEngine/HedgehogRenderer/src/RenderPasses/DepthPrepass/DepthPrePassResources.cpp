#include "DepthPrePassResources.hpp"

#include "RenderPasses/PassInitContext.hpp"

#include "Pipeline/ShaderLoader.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHIRenderPass.hpp"
#include "RHI/api/IRHIPipeline.hpp"
#include "RHI/api/IRHIDescriptor.hpp"

#include <cassert>

namespace Renderer
{
    DepthPrePassResources::DepthPrePassResources(const PassInitContext& init)
    {
        const auto sd = ShaderLoader::Load(init.Device,
            "engine://HedgehogEngine/HedgehogRenderer/assets/Shaders/DepthPrepass.shader",
            init.FileSystem);
        assert(!sd.Layout.DescriptorSets.empty());

        m_FrameBindings = sd.Layout.DescriptorSets[0];

        m_FrameLayout = init.Device.CreateDescriptorSetLayout(sd.Layout.DescriptorSets[0]);

        // Render pass: depth-only (no colour attachments). Format is the device's preferred depth
        // format, cached here so DepthPrePass::Setup() can declare its viewDepth graph texture
        // with a matching format.
        m_DepthFormat = init.Device.GetPreferredDepthFormat();

        RHI::RenderPassDesc rpDesc;
        rpDesc.DepthAttachment = RHI::AttachmentDesc{
            m_DepthFormat,
            RHI::LoadOp::Clear,
            RHI::StoreOp::Store,
            RHI::LoadOp::DontCare,
            RHI::StoreOp::DontCare,
            RHI::ImageLayout::Undefined,
            RHI::ImageLayout::DepthStencilReadOnly
        };
        m_RenderPass = init.Device.CreateRenderPass(rpDesc);

        // Pipeline
        auto pipelineDesc                 = sd.Pipeline;
        pipelineDesc.DescriptorSetLayouts = { m_FrameLayout.get() };
        pipelineDesc.RenderPass           = m_RenderPass.get();
        m_Pipeline = init.Device.CreateGraphicsPipeline(pipelineDesc);
    }

    DepthPrePassResources::~DepthPrePassResources()
    {
    }
}
