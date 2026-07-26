#include "DepthPrePass.hpp"
#include "DepthPrePassPushConstants.hpp"
#include "DepthPrePassResources.hpp"

#include "RenderPasses/PassInitContext.hpp"
#include "RenderPasses/PassResourceCache.hpp"

#include "RenderGraph/RenderGraph.hpp"
#include "RenderGraph/RenderGraphBuilder.hpp"
#include "RenderGraph/RenderGraphTypes.hpp"

#include "HedgehogCommon/api/Frame/FrameData.hpp"

#include "ResourceRegistry/ResourceRegistry.hpp"
#include "ResourceRegistry/MeshGpuData.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"

#include "Pipeline/PipelineLoader.hpp"

#include "Profiling/Profiler.hpp"

#include <cassert>

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHIRenderPass.hpp"
#include "RHI/api/IRHIFramebuffer.hpp"
#include "RHI/api/IRHIPipeline.hpp"
#include "RHI/api/IRHIDescriptor.hpp"
#include "RHI/api/IRHIBuffer.hpp"
#include "RHI/api/IRHITexture.hpp"

namespace Renderer
{
namespace
{
    const HedgehogEngine::DrawBucket s_EmptyBucket; // view has no camera this frame: clear only
}

    DepthPrePass::DepthPrePass(const PassInitContext& init)
        : m_Resources(init.Cache.GetOrCreate<DepthPrePassResources>("DepthPrePass", init))
    {
        m_FramePool = init.Device.CreateDescriptorPool(
            HedgehogEngine::MAX_FRAMES_IN_FLIGHT,
            PipelineLoader::MakePoolSizes(m_Resources->GetFrameBindings(), HedgehogEngine::MAX_FRAMES_IN_FLIGHT));

        m_FrameUniforms.reserve(HedgehogEngine::MAX_FRAMES_IN_FLIGHT);
        m_FrameSets.reserve(HedgehogEngine::MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < HedgehogEngine::MAX_FRAMES_IN_FLIGHT; ++i)
        {
            auto ubo = init.Device.CreateBuffer(
                sizeof(DepthPrepassFrameUniform),
                RHI::BufferUsage::UniformBuffer,
                RHI::MemoryUsage::CpuToGpu);

            auto set = init.Device.AllocateDescriptorSet(*m_FramePool, m_Resources->GetFrameLayout());
            set->WriteUniformBuffer(0, *ubo);
            set->Flush();

            m_FrameUniforms.push_back(std::move(ubo));
            m_FrameSets.push_back(std::move(set));
        }
    }

    DepthPrePass::~DepthPrePass()
    {
    }

    void DepthPrePass::Setup(RenderGraphBuilder& builder)
    {
        GraphTextureDesc desc;
        desc.TextureSizeClass = SizeClass::ViewRelative;
        desc.Format           = m_Resources->GetDepthFormat();
        desc.Usage            = RHI::TextureUsage::DepthStencil;
        builder.CreateTexture(GraphResourceNames::VIEW_DEPTH, desc);
        builder.Write(GraphResourceNames::VIEW_DEPTH, RHI::ImageLayout::DepthStencilReadOnly);
    }

    void DepthPrePass::CreateFramebuffers(RHI::IRHIDevice& device, RenderGraph& graph)
    {
        m_FrameBuffer.reset();

        auto& depthBuffer = graph.GetTexture(GraphResourceNames::VIEW_DEPTH);

        RHI::FramebufferDesc fbDesc;
        fbDesc.RenderPass      = &m_Resources->GetRenderPass();
        fbDesc.DepthAttachment = &depthBuffer;
        fbDesc.Width           = depthBuffer.GetWidth();
        fbDesc.Height          = depthBuffer.GetHeight();
        m_FrameBuffer = device.CreateFramebuffer(fbDesc);
    }

    void DepthPrePass::Execute(RenderGraphContext& ctx)
    {
        HH_PROFILE_ZONE("DepthPrePass");

        const HedgehogEngine::CameraData& camera = ctx.View->Camera;
        const HedgehogEngine::DrawBucket& opaque = ctx.View->HasCamera ? ctx.FrameData->DrawList.Opaque : s_EmptyBucket;

        DepthPrepassFrameUniform ubo{};
        ubo.ViewProj = camera.Proj * camera.View;
        m_FrameUniforms[ctx.FrameIndex]->CopyData(&ubo, sizeof(ubo));

        RHI::ClearValue depthClear;
        depthClear.IsDepth      = true;
        depthClear.DepthStencil = { 1.0f, 0 };

        auto& cmd = *ctx.CommandList;

        cmd.BeginRenderPass(m_Resources->GetRenderPass(), *m_FrameBuffer, { depthClear });

        const uint32_t width  = m_FrameBuffer->GetWidth();
        const uint32_t height = m_FrameBuffer->GetHeight();

        cmd.BindPipeline(m_Resources->GetPipeline());
        cmd.SetViewport({ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f });
        cmd.SetScissor({ 0, 0, width, height });

        auto& registry  = *ctx.ResourceRegistry;
        auto& posBuffer = const_cast<RHI::IRHIBuffer&>(registry.GetPositionsBuffer());
        cmd.BindVertexBuffers(0, { &posBuffer }, { 0 });

        auto& idxBuffer = const_cast<RHI::IRHIBuffer&>(registry.GetIndexBuffer());
        cmd.BindIndexBuffer(idxBuffer, RHI::IndexType::Uint32);

        cmd.BindDescriptorSet(m_Resources->GetPipeline(), 0, *m_FrameSets[ctx.FrameIndex]);

        for (const auto& drawNode : opaque)
        {
            for (const auto& object : drawNode.Objects)
            {
                cmd.PushConstants(
                    m_Resources->GetPipeline(),
                    RHI::ShaderStage::Vertex,
                    0,
                    static_cast<uint32_t>(sizeof(DepthPrePassPushConstants)),
                    &object.Transform);

                const auto& geom = registry.GetMeshGeometryInfo(object.MeshIndex);
                cmd.DrawIndexed(geom.IndexCount, 1, geom.FirstIndex, geom.VertexOffset, 0);
            }
        }

        cmd.EndRenderPass();
    }

    void DepthPrePass::Cleanup(RHI::IRHIDevice& device)
    {
        device.WaitIdle();

        m_FrameSets.clear();
        m_FrameUniforms.clear();
        m_FrameBuffer.reset();
        m_FramePool.reset();
        m_Resources.reset();
    }

}
