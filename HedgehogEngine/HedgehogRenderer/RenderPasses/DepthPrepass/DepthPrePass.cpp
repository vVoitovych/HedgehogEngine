#include "DepthPrePass.hpp"
#include "DepthPrePassPushConstants.hpp"

#include "HedgehogEngine/api/Frame/FrameData.hpp"

#include "HedgehogRenderer/ResourceManager/ResourceManager.hpp"
#include "HedgehogRenderer/ResourceRegistry/ResourceRegistry.hpp"
#include "HedgehogRenderer/ResourceRegistry/MeshGpuData.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHIRenderPass.hpp"
#include "RHI/api/IRHIFramebuffer.hpp"
#include "RHI/api/IRHIPipeline.hpp"
#include "RHI/api/IRHIDescriptor.hpp"
#include "RHI/api/IRHIBuffer.hpp"
#include "RHI/api/IRHITexture.hpp"
#include "RHI/api/IRHIShader.hpp"

namespace Renderer
{

    DepthPrePass::DepthPrePass(RHI::IRHIDevice& device, const ResourceManager& resourceManager)
    {
        // Descriptor set layout: binding 0 = uniform buffer (vertex stage)
        m_FrameLayout = device.CreateDescriptorSetLayout({
            { 0, RHI::DescriptorType::UniformBuffer, 1, RHI::ShaderStage::Vertex }
        });

        // Descriptor pool: one UB per frame
        m_FramePool = device.CreateDescriptorPool(
            MAX_FRAMES_IN_FLIGHT,
            { { RHI::DescriptorType::UniformBuffer, MAX_FRAMES_IN_FLIGHT } });

        // Per-frame uniform buffers and descriptor sets
        m_FrameUniforms.reserve(MAX_FRAMES_IN_FLIGHT);
        m_FrameSets.reserve(MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            auto ubo = device.CreateBuffer(
                sizeof(DepthPrepassFrameUniform),
                RHI::BufferUsage::UniformBuffer,
                RHI::MemoryUsage::CpuToGpu);

            auto set = device.AllocateDescriptorSet(*m_FramePool, *m_FrameLayout);
            set->WriteUniformBuffer(0, *ubo);
            set->Flush();

            m_FrameUniforms.push_back(std::move(ubo));
            m_FrameSets.push_back(std::move(set));
        }

        // Render pass: depth-only (no color attachments)
        RHI::RenderPassDesc rpDesc;
        rpDesc.m_DepthAttachment = RHI::AttachmentDesc{
            resourceManager.GetRHIDepthBuffer().GetFormat(),
            RHI::LoadOp::Clear,
            RHI::StoreOp::Store,
            RHI::LoadOp::DontCare,
            RHI::StoreOp::DontCare,
            RHI::ImageLayout::Undefined,
            RHI::ImageLayout::DepthStencilReadOnly
        };
        m_RenderPass = device.CreateRenderPass(rpDesc);

        // Shaders
        auto vertexShader = device.CreateShader(
            "/Shaders/Shaders/DepthPrepass/Base.vert.spv",
            RHI::ShaderStage::Vertex);

        // Pipeline
        RHI::GraphicsPipelineDesc pipelineDesc;
        pipelineDesc.m_VertexShader   = vertexShader.get();
        pipelineDesc.m_FragmentShader = nullptr; // depth-only pass

        pipelineDesc.m_VertexBindings  = { { 0, 3 * sizeof(float), RHI::VertexInputRate::PerVertex } };
        pipelineDesc.m_VertexAttributes = { { 0, 0, RHI::Format::R32G32B32Float, 0 } };

        pipelineDesc.m_Topology          = RHI::PrimitiveTopology::TriangleList;
        pipelineDesc.m_CullMode          = RHI::CullMode::Back;
        pipelineDesc.m_FillMode          = RHI::FillMode::Solid;
        pipelineDesc.m_DepthTestEnable   = true;
        pipelineDesc.m_DepthWriteEnable  = true;
        pipelineDesc.m_DepthCompareOp    = RHI::CompareOp::Less;

        pipelineDesc.m_DescriptorSetLayouts = { m_FrameLayout.get() };
        pipelineDesc.m_PushConstantRanges   = {
            { RHI::ShaderStage::Vertex, 0, static_cast<uint32_t>(sizeof(DepthPrePassPushConstants)) }
        };
        pipelineDesc.m_RenderPass = m_RenderPass.get();

        m_Pipeline = device.CreateGraphicsPipeline(pipelineDesc);

        // Framebuffer
        const auto& depthBuffer = resourceManager.GetRHIDepthBuffer();
        RHI::FramebufferDesc fbDesc;
        fbDesc.m_RenderPass      = m_RenderPass.get();
        fbDesc.m_DepthAttachment = &depthBuffer;
        fbDesc.m_Width           = depthBuffer.GetWidth();
        fbDesc.m_Height          = depthBuffer.GetHeight();
        m_FrameBuffer = device.CreateFramebuffer(fbDesc);
    }

    DepthPrePass::~DepthPrePass()
    {
    }

    void DepthPrePass::Render(const HedgehogEngine::FrameData& frame, const ResourceManager& resourceManager,
                               RHI::IRHICommandList& cmd, uint32_t frameIndex)
    {
        // Update frame uniform
        DepthPrepassFrameUniform ubo{};
        ubo.m_ViewProj = frame.m_Camera.m_Proj * frame.m_Camera.m_View;
        m_FrameUniforms[frameIndex]->CopyData(&ubo, sizeof(ubo));

        // Begin render pass
        RHI::ClearValue depthClear;
        depthClear.m_IsDepth      = true;
        depthClear.m_DepthStencil = { 1.0f, 0 };

        cmd.BeginRenderPass(*m_RenderPass, *m_FrameBuffer, { depthClear });

        const uint32_t width  = m_FrameBuffer->GetWidth();
        const uint32_t height = m_FrameBuffer->GetHeight();

        cmd.BindPipeline(*m_Pipeline);
        cmd.SetViewport({ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f });
        cmd.SetScissor({ 0, 0, width, height });

        auto& registry  = resourceManager.GetResourceRegistry();
        auto& posBuffer = const_cast<RHI::IRHIBuffer&>(registry.GetPositionsBuffer());
        cmd.BindVertexBuffers(0, { &posBuffer }, { 0 });

        auto& idxBuffer = const_cast<RHI::IRHIBuffer&>(registry.GetIndexBuffer());
        cmd.BindIndexBuffer(idxBuffer, RHI::IndexType::Uint32);

        cmd.BindDescriptorSet(*m_Pipeline, 0, *m_FrameSets[frameIndex]);

        for (const auto& drawNode : frame.m_DrawList.m_Opaque)
        {
            for (const auto& object : drawNode.m_Objects)
            {
                cmd.PushConstants(
                    *m_Pipeline,
                    RHI::ShaderStage::Vertex,
                    0,
                    static_cast<uint32_t>(sizeof(DepthPrePassPushConstants)),
                    &object.m_Transform);

                const auto& geom = registry.GetMeshGeometryInfo(object.m_MeshIndex);
                cmd.DrawIndexed(geom.m_IndexCount, 1, geom.m_FirstIndex, geom.m_VertexOffset, 0);
            }
        }

        cmd.EndRenderPass();
    }

    void DepthPrePass::Cleanup(RHI::IRHIDevice& device)
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

    void DepthPrePass::ResizeResources(RHI::IRHIDevice& device, const ResourceManager& resourceManager)
    {
        const auto& depthBuffer = resourceManager.GetRHIDepthBuffer();

        m_FrameBuffer.reset();

        RHI::FramebufferDesc fbDesc;
        fbDesc.m_RenderPass      = m_RenderPass.get();
        fbDesc.m_DepthAttachment = &depthBuffer;
        fbDesc.m_Width           = depthBuffer.GetWidth();
        fbDesc.m_Height          = depthBuffer.GetHeight();
        m_FrameBuffer = device.CreateFramebuffer(fbDesc);
    }

}
