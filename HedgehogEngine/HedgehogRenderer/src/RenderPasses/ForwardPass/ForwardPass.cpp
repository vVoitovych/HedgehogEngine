#include "ForwardPass.hpp"
#include "ForwardPassPushConstants.hpp"
#include "ForwardPassResources.hpp"

#include "RenderPasses/PassInitContext.hpp"
#include "RenderPasses/PassResourceCache.hpp"

#include "RenderGraph/RenderGraph.hpp"
#include "RenderGraph/RenderGraphBuilder.hpp"
#include "RenderGraph/RenderGraphTypes.hpp"

#include "HedgehogCommon/api/Frame/FrameData.hpp"

#include "ResourceRegistry/ResourceRegistry.hpp"
#include "ResourceRegistry/MeshGpuData.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"
#include "HedgehogMath/api/Common.hpp"

#include "Pipeline/PipelineLoader.hpp"

#include "Profiling/Profiler.hpp"

#include <cassert>
#include "HedgehogMath/api/Vector.hpp"

#include <cmath>

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

    ForwardPass::GpuLight ForwardPass::ToGpuLight(const HedgehogEngine::LightData& fd)
    {
        GpuLight gpu;
        gpu.Position  = fd.Position;
        gpu.Direction = fd.Direction;
        gpu.Color     = fd.Color;
        gpu.Data      = HM::Vector4(
            static_cast<float>(fd.Type),
            fd.Intensity,
            fd.Radius,
            std::cos(HM::ToRadians(fd.ConeAngle)));
        return gpu;
    }

    ForwardPass::ForwardPass(const PassInitContext& init)
        : m_Resources(init.Cache.GetOrCreate<ForwardPassResources>("ForwardPass", init))
    {
        m_FramePool = init.Device.CreateDescriptorPool(
            HedgehogEngine::MAX_FRAMES_IN_FLIGHT,
            PipelineLoader::MakePoolSizes(m_Resources->GetFrameBindings(), HedgehogEngine::MAX_FRAMES_IN_FLIGHT));

        m_FrameUniforms.reserve(HedgehogEngine::MAX_FRAMES_IN_FLIGHT);
        m_FrameSets.reserve(HedgehogEngine::MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < HedgehogEngine::MAX_FRAMES_IN_FLIGHT; ++i)
        {
            auto ubo = init.Device.CreateBuffer(
                sizeof(ForwardPassFrameUniform),
                RHI::BufferUsage::UniformBuffer,
                RHI::MemoryUsage::CpuToGpu);

            auto set = init.Device.AllocateDescriptorSet(*m_FramePool, m_Resources->GetFrameLayout());
            set->WriteUniformBuffer(0, *ubo);
            set->Flush();

            m_FrameUniforms.push_back(std::move(ubo));
            m_FrameSets.push_back(std::move(set));
        }
    }

    ForwardPass::~ForwardPass()
    {
    }

    void ForwardPass::Setup(RenderGraphBuilder& builder)
    {
        // Dependency-only: the depth handoff from DepthPrePass is handled by the render passes'
        // matching initial/final layouts (DepthStencilReadOnly both sides), so no auto-barrier
        // is needed here.
        builder.Read(GraphResourceNames::VIEW_DEPTH);

        GraphTextureDesc desc;
        desc.TextureSizeClass = SizeClass::ViewRelative;
        desc.Format           = m_Resources->GetColorFormat();
        // TransferSrc alongside ColorAttachment|Sampled: a composition pipeline with no GuiPass
        // (e.g. present_direct.rgq) blits this view's colour straight to the swapchain, so it must
        // itself be a valid blit source — not only sampled by GuiPass, as in composition_editor.rgq.
        desc.Usage = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled | RHI::TextureUsage::TransferSrc;
        builder.CreateTexture(GraphResourceNames::VIEW_COLOR, desc);
        builder.Write(GraphResourceNames::VIEW_COLOR, RHI::ImageLayout::ColorAttachment);
    }

    void ForwardPass::CreateFramebuffers(RHI::IRHIDevice& device, RenderGraph& graph)
    {
        m_FrameBuffer.reset();

        auto& colorBuffer = graph.GetTexture(GraphResourceNames::VIEW_COLOR);
        auto& depthBuffer = graph.GetTexture(GraphResourceNames::VIEW_DEPTH);

        RHI::FramebufferDesc fbDesc;
        fbDesc.RenderPass       = &m_Resources->GetRenderPass();
        fbDesc.ColorAttachments = { &colorBuffer };
        fbDesc.DepthAttachment  = &depthBuffer;
        fbDesc.Width            = colorBuffer.GetWidth();
        fbDesc.Height           = colorBuffer.GetHeight();
        m_FrameBuffer = device.CreateFramebuffer(fbDesc);
    }

    void ForwardPass::Execute(RenderGraphContext& ctx)
    {
        HH_PROFILE_ZONE("ForwardPass");

        const HedgehogEngine::CameraData& camera = ctx.View->Camera;
        const HedgehogEngine::DrawBucket& opaque = ctx.View->HasCamera ? ctx.FrameData->DrawList.Opaque : s_EmptyBucket;
        const auto& lights = ctx.FrameData->Lights;

        ForwardPassFrameUniform ubo{};
        ubo.View        = camera.View;
        ubo.ViewProj    = camera.Proj * camera.View;
        ubo.EyePosition = camera.Position;
        ubo.LightCount  = lights.size();
        for (size_t i = 0; i < ubo.LightCount; ++i)
            ubo.Lights[i] = ToGpuLight(lights[i]);
        m_FrameUniforms[ctx.FrameIndex]->CopyData(&ubo, sizeof(ubo));

        RHI::ClearValue colorClear;
        colorClear.Color = { 0.0f, 0.0f, 0.0f, 1.0f };
        RHI::ClearValue depthClear;
        depthClear.IsDepth      = true;
        depthClear.DepthStencil = { 1.0f, 0 };

        auto& cmd = *ctx.CommandList;

        cmd.BeginRenderPass(m_Resources->GetRenderPass(), *m_FrameBuffer, { colorClear, depthClear });

        const uint32_t width  = m_FrameBuffer->GetWidth();
        const uint32_t height = m_FrameBuffer->GetHeight();

        cmd.BindPipeline(m_Resources->GetPipeline());
        cmd.SetViewport({ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f });
        cmd.SetScissor({ 0, 0, width, height });

        auto& registry  = *ctx.ResourceRegistry;
        auto& posBuffer = const_cast<RHI::IRHIBuffer&>(registry.GetPositionsBuffer());
        auto& uvBuffer  = const_cast<RHI::IRHIBuffer&>(registry.GetTexCoordsBuffer());
        auto& nrmBuffer = const_cast<RHI::IRHIBuffer&>(registry.GetNormalsBuffer());
        auto& idxBuffer = const_cast<RHI::IRHIBuffer&>(registry.GetIndexBuffer());

        cmd.BindVertexBuffers(0, { &posBuffer, &uvBuffer, &nrmBuffer }, { 0, 0, 0 });
        cmd.BindIndexBuffer(idxBuffer, RHI::IndexType::Uint32);

        cmd.BindDescriptorSet(m_Resources->GetPipeline(), 0, *m_FrameSets[ctx.FrameIndex]);

        for (const auto& drawNode : opaque)
        {
            cmd.BindDescriptorSet(
                m_Resources->GetPipeline(), 1, registry.GetMaterialDescriptorSet(static_cast<uint32_t>(drawNode.MaterialIndex)));

            for (const auto& object : drawNode.Objects)
            {
                cmd.PushConstants(
                    m_Resources->GetPipeline(),
                    RHI::ShaderStage::Vertex,
                    0,
                    static_cast<uint32_t>(sizeof(ForwardPassPushConstants)),
                    &object.Transform);

                const auto& geom = registry.GetMeshGeometryInfo(object.MeshIndex);
                cmd.DrawIndexed(geom.IndexCount, 1, geom.FirstIndex, geom.VertexOffset, 0);
            }
        }

        cmd.EndRenderPass();
    }

    void ForwardPass::Cleanup(RHI::IRHIDevice& device)
    {
        device.WaitIdle();

        m_FrameSets.clear();
        m_FrameUniforms.clear();
        m_FrameBuffer.reset();
        m_FramePool.reset();
        m_Resources.reset();
    }

}
