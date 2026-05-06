#include "DepthPrepassNode.hpp"

#include "../RenderContext.hpp"
#include "../../ResourceManager/ResourceManager.hpp"
#include "../../ResourceManager/ResourceNames.hpp"
#include "../../ResourceRegistry/ResourceRegistry.hpp"
#include "../../ResourceRegistry/MeshGpuData.hpp"
#include "../../Pipeline/ShaderLoader.hpp"
#include "../../Pipeline/PipelineLoader.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"
#include "HedgehogEngine/api/Frame/FrameData.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHIRenderPass.hpp"
#include "RHI/api/IRHIFramebuffer.hpp"
#include "RHI/api/IRHIPipeline.hpp"
#include "RHI/api/IRHIDescriptor.hpp"
#include "RHI/api/IRHIBuffer.hpp"
#include "RHI/api/IRHITexture.hpp"

#include <cassert>

namespace Renderer
{
    namespace
    {
        struct PushConstants { HM::Matrix4x4 objToWorld; };
    }

    DepthPrepassNode::DepthPrepassNode(RHI::IRHIDevice& device, const ResourceManager& resourceManager)
    {
        const auto sd = ShaderLoader::Load(device,
            "/HedgehogEngine/HedgehogRenderer/Assets/Shaders/DepthPrepass.shader");
        assert(!sd.m_Layout.m_DescriptorSets.empty());

        m_FrameLayout = device.CreateDescriptorSetLayout(sd.m_Layout.m_DescriptorSets[0]);
        m_FramePool   = device.CreateDescriptorPool(
            MAX_FRAMES_IN_FLIGHT,
            PipelineLoader::MakePoolSizes(sd.m_Layout.m_DescriptorSets[0], MAX_FRAMES_IN_FLIGHT));

        m_FrameUniforms.reserve(MAX_FRAMES_IN_FLIGHT);
        m_FrameSets.reserve(MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            auto ubo = device.CreateBuffer(
                sizeof(FrameUniform), RHI::BufferUsage::UniformBuffer, RHI::MemoryUsage::CpuToGpu);
            auto set = device.AllocateDescriptorSet(*m_FramePool, *m_FrameLayout);
            set->WriteUniformBuffer(0, *ubo);
            set->Flush();
            m_FrameUniforms.push_back(std::move(ubo));
            m_FrameSets.push_back(std::move(set));
        }

        const auto& depthBuffer = resourceManager.GetTexture(ResourceNames::RHIDepthBuffer);

        RHI::RenderPassDesc rpDesc;
        rpDesc.m_DepthAttachment = RHI::AttachmentDesc{
            depthBuffer.GetFormat(),
            RHI::LoadOp::Clear,    RHI::StoreOp::Store,
            RHI::LoadOp::DontCare, RHI::StoreOp::DontCare,
            RHI::ImageLayout::Undefined, RHI::ImageLayout::DepthStencilReadOnly
        };
        m_RenderPass = device.CreateRenderPass(rpDesc);

        auto pipelineDesc                   = sd.m_Pipeline;
        pipelineDesc.m_DescriptorSetLayouts = { m_FrameLayout.get() };
        pipelineDesc.m_RenderPass           = m_RenderPass.get();
        m_Pipeline = device.CreateGraphicsPipeline(pipelineDesc);

        RHI::FramebufferDesc fbDesc;
        fbDesc.m_RenderPass      = m_RenderPass.get();
        fbDesc.m_DepthAttachment = &depthBuffer;
        fbDesc.m_Width           = depthBuffer.GetWidth();
        fbDesc.m_Height          = depthBuffer.GetHeight();
        m_FrameBuffer = device.CreateFramebuffer(fbDesc);

        m_CachedWidth  = depthBuffer.GetWidth();
        m_CachedHeight = depthBuffer.GetHeight();
    }

    DepthPrepassNode::~DepthPrepassNode() = default;

    void DepthPrepassNode::Render(RenderContext& ctx)
    {
        RebuildFramebufferIfNeeded(ctx.m_Device.get(), ctx.m_ResourceManager.get());

        const auto& frame = ctx.m_FrameData.get();
        auto&       cmd   = ctx.m_Cmd.get();
        const auto& rm    = ctx.m_ResourceManager.get();

        FrameUniform ubo{};
        ubo.m_ViewProj = frame.m_Camera.m_Proj * frame.m_Camera.m_View;
        m_FrameUniforms[ctx.m_FrameIndex]->CopyData(&ubo, sizeof(ubo));

        RHI::ClearValue depthClear;
        depthClear.m_IsDepth      = true;
        depthClear.m_DepthStencil = { 1.0f, 0 };

        cmd.BeginRenderPass(*m_RenderPass, *m_FrameBuffer, { depthClear });

        const uint32_t w = m_FrameBuffer->GetWidth();
        const uint32_t h = m_FrameBuffer->GetHeight();

        cmd.BindPipeline(*m_Pipeline);
        cmd.SetViewport({ 0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f, 1.0f });
        cmd.SetScissor({ 0, 0, w, h });

        auto& registry  = rm.GetResourceRegistry();
        auto& posBuffer = const_cast<RHI::IRHIBuffer&>(registry.GetPositionsBuffer());
        auto& idxBuffer = const_cast<RHI::IRHIBuffer&>(registry.GetIndexBuffer());

        cmd.BindVertexBuffers(0, { &posBuffer }, { 0 });
        cmd.BindIndexBuffer(idxBuffer, RHI::IndexType::Uint32);
        cmd.BindDescriptorSet(*m_Pipeline, 0, *m_FrameSets[ctx.m_FrameIndex]);

        for (const auto& drawNode : frame.m_DrawList.m_Opaque)
        {
            for (const auto& object : drawNode.m_Objects)
            {
                cmd.PushConstants(*m_Pipeline, RHI::ShaderStage::Vertex,
                    0, static_cast<uint32_t>(sizeof(PushConstants)), &object.m_Transform);

                const auto& geom = registry.GetMeshGeometryInfo(object.m_MeshIndex);
                cmd.DrawIndexed(geom.m_IndexCount, 1, geom.m_FirstIndex, geom.m_VertexOffset, 0);
            }
        }

        cmd.EndRenderPass();
    }

    void DepthPrepassNode::Cleanup(RHI::IRHIDevice& device)
    {
        device.WaitIdle();
        m_FrameSets.clear();
        m_FrameUniforms.clear();
        m_Pipeline.reset();
        m_FrameBuffer.reset();
        m_RenderPass.reset();
        m_FramePool.reset();
        m_FrameLayout.reset();
    }

    void DepthPrepassNode::RebuildFramebufferIfNeeded(RHI::IRHIDevice& device, const ResourceManager& rm)
    {
        const auto& depthTex = rm.GetTexture(ResourceNames::RHIDepthBuffer);
        if (depthTex.GetWidth() == m_CachedWidth && depthTex.GetHeight() == m_CachedHeight)
            return;

        m_FrameBuffer.reset();

        RHI::FramebufferDesc fbDesc;
        fbDesc.m_RenderPass      = m_RenderPass.get();
        fbDesc.m_DepthAttachment = &depthTex;
        fbDesc.m_Width           = depthTex.GetWidth();
        fbDesc.m_Height          = depthTex.GetHeight();
        m_FrameBuffer = device.CreateFramebuffer(fbDesc);

        m_CachedWidth  = depthTex.GetWidth();
        m_CachedHeight = depthTex.GetHeight();
    }

} // namespace Renderer
