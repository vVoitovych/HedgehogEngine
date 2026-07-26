#include "ForwardPassResources.hpp"

#include "RenderPasses/PassInitContext.hpp"

#include "ResourceRegistry/ResourceRegistry.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"

#include "Pipeline/ShaderLoader.hpp"
#include "Pipeline/PipelineLoader.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHIRenderPass.hpp"
#include "RHI/api/IRHIPipeline.hpp"
#include "RHI/api/IRHIDescriptor.hpp"

#include <cassert>

namespace Renderer
{
    ForwardPassResources::ForwardPassResources(const PassInitContext& init)
    {
        const auto sd = ShaderLoader::Load(init.Device,
            "engine://HedgehogEngine/HedgehogRenderer/assets/Shaders/ForwardPass.shader",
            init.FileSystem);
        assert(sd.Layout.DescriptorSets.size() >= 2);

        m_FrameBindings = sd.Layout.DescriptorSets[0];

        // Set 0: per-frame data (camera, lights)
        m_FrameLayout = init.Device.CreateDescriptorSetLayout(sd.Layout.DescriptorSets[0]);

        // Set 1: per-material data — ForwardPass defines and owns this layout. The layout is
        // injected into ResourceRegistry so it can allocate material descriptor sets; this is
        // the one-time registration ResourceRegistry::SetMaterialLayout asserts against, now run
        // exactly once here regardless of how many ForwardPass instances exist.
        m_MaterialLayout = init.Device.CreateDescriptorSetLayout(sd.Layout.DescriptorSets[1]);
        init.Registry.SetMaterialLayout(
            init.Device,
            *m_MaterialLayout,
            HedgehogEngine::MAX_MATERIAL_COUNT,
            PipelineLoader::MakePoolSizes(sd.Layout.DescriptorSets[1], HedgehogEngine::MAX_MATERIAL_COUNT));

        // Render pass: one colour + depth (loaded from DepthPrepass). Colour format is a fixed
        // renderer-wide convention for every view's colour target (see RenderView/ForwardPass);
        // depth format is the device's preferred depth format, cached here so ForwardPass::Setup()
        // can declare its viewColor/viewDepth graph textures with matching formats.
        m_ColorFormat = RHI::Format::R16G16B16A16Unorm;
        m_DepthFormat = init.Device.GetPreferredDepthFormat();

        RHI::RenderPassDesc rpDesc;
        rpDesc.ColorAttachments.push_back(RHI::AttachmentDesc{
            m_ColorFormat,
            RHI::LoadOp::Clear,
            RHI::StoreOp::Store,
            RHI::LoadOp::DontCare,
            RHI::StoreOp::DontCare,
            RHI::ImageLayout::Undefined,
            RHI::ImageLayout::ColorAttachment
        });
        rpDesc.DepthAttachment = RHI::AttachmentDesc{
            m_DepthFormat,
            RHI::LoadOp::Load,
            RHI::StoreOp::DontCare,
            RHI::LoadOp::DontCare,
            RHI::StoreOp::DontCare,
            RHI::ImageLayout::DepthStencilReadOnly,
            RHI::ImageLayout::DepthStencilReadOnly
        };
        m_RenderPass = init.Device.CreateRenderPass(rpDesc);

        // Pipeline
        auto pipelineDesc                 = sd.Pipeline;
        pipelineDesc.DescriptorSetLayouts = { m_FrameLayout.get(), m_MaterialLayout.get() };
        pipelineDesc.RenderPass           = m_RenderPass.get();
        m_Pipeline = init.Device.CreateGraphicsPipeline(pipelineDesc);
    }

    ForwardPassResources::~ForwardPassResources()
    {
    }
}
