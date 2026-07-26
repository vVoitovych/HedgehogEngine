#include "ShadowmapPassResources.hpp"

#include "RenderPasses/PassInitContext.hpp"

#include "Pipeline/ShaderLoader.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHIRenderPass.hpp"
#include "RHI/api/IRHIPipeline.hpp"
#include "RHI/api/IRHIDescriptor.hpp"

#include <cassert>

namespace Renderer
{
    ShadowmapPassResources::ShadowmapPassResources(const PassInitContext& init)
    {
        const auto sd = ShaderLoader::Load(init.Device,
            "engine://HedgehogEngine/HedgehogRenderer/assets/Shaders/ShadowmapPass.shader",
            init.FileSystem);
        assert(!sd.Layout.DescriptorSets.empty());

        m_ShadowBindings = sd.Layout.DescriptorSets[0];

        m_ShadowmapLayout = init.Device.CreateDescriptorSetLayout(sd.Layout.DescriptorSets[0]);

        // Render pass: depth-only, Clear/Store, Undefined → DepthStencilReadOnly. Format is the
        // device's preferred depth format, cached here so ShadowmapPass::Setup() can declare its
        // shadowDepth graph texture with a matching format.
        m_ShadowFormat = init.Device.GetPreferredDepthFormat();

        RHI::RenderPassDesc rpDesc;
        rpDesc.DepthAttachment = RHI::AttachmentDesc{
            m_ShadowFormat,
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
        pipelineDesc.DescriptorSetLayouts = { m_ShadowmapLayout.get() };
        pipelineDesc.RenderPass           = m_RenderPass.get();
        m_Pipeline = init.Device.CreateGraphicsPipeline(pipelineDesc);
    }

    ShadowmapPassResources::~ShadowmapPassResources()
    {
    }
}
