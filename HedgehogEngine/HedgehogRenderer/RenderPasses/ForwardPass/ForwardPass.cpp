#include "ForwardPass.hpp"
#include "ForwardPassPushConstants.hpp"

#include "FrameData/FrameData.hpp"

#include "HedgehogRenderer/ResourceManager/ResourceManager.hpp"
#include "HedgehogRenderer/ResourceRegistry/ResourceRegistry.hpp"
#include "HedgehogRenderer/ResourceRegistry/MeshGpuData.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"
#include "HedgehogMath/api/Common.hpp"
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
#include "RHI/api/IRHIShader.hpp"

namespace Renderer
{
    ForwardPass::GpuLight ForwardPass::ToGpuLight(const FD::LightData& fd)
    {
        GpuLight gpu;
        gpu.m_Position  = fd.m_Position;
        gpu.m_Direction = fd.m_Direction;
        gpu.m_Color     = fd.m_Color;
        gpu.m_Data      = HM::Vector4(
            static_cast<float>(fd.m_Type),
            fd.m_Intensity,
            fd.m_Radius,
            std::cos(HM::ToRadians(fd.m_ConeAngle)));
        return gpu;
    }


    ForwardPass::ForwardPass(RHI::IRHIDevice& device, const ResourceManager& resourceManager)
    {
        auto& registry  = resourceManager.GetResourceRegistry();

        // Frame descriptor set layout: binding 0 = UB (vertex + fragment)
        m_FrameLayout = device.CreateDescriptorSetLayout({
            { 0, RHI::DescriptorType::UniformBuffer, 1, RHI::ShaderStage::Vertex | RHI::ShaderStage::Fragment }
        });

        m_FramePool = device.CreateDescriptorPool(
            MAX_FRAMES_IN_FLIGHT,
            { { RHI::DescriptorType::UniformBuffer, MAX_FRAMES_IN_FLIGHT } });

        m_FrameUniforms.reserve(MAX_FRAMES_IN_FLIGHT);
        m_FrameSets.reserve(MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            auto ubo = device.CreateBuffer(
                sizeof(ForwardPassFrameUniform),
                RHI::BufferUsage::UniformBuffer,
                RHI::MemoryUsage::CpuToGpu);

            auto set = device.AllocateDescriptorSet(*m_FramePool, *m_FrameLayout);
            set->WriteUniformBuffer(0, *ubo);
            set->Flush();

            m_FrameUniforms.push_back(std::move(ubo));
            m_FrameSets.push_back(std::move(set));
        }

        // Render pass: one color + depth (loaded from DepthPrePass)
        RHI::RenderPassDesc rpDesc;
        rpDesc.m_ColorAttachments.push_back(RHI::AttachmentDesc{
            resourceManager.GetSceneColorBuffer().GetFormat(),
            RHI::LoadOp::Clear,
            RHI::StoreOp::Store,
            RHI::LoadOp::DontCare,
            RHI::StoreOp::DontCare,
            RHI::ImageLayout::Undefined,
            RHI::ImageLayout::ColorAttachment
        });
        rpDesc.m_DepthAttachment = RHI::AttachmentDesc{
            resourceManager.GetRHIDepthBuffer().GetFormat(),
            RHI::LoadOp::Load,
            RHI::StoreOp::DontCare,
            RHI::LoadOp::DontCare,
            RHI::StoreOp::DontCare,
            RHI::ImageLayout::DepthStencilReadOnly,
            RHI::ImageLayout::DepthStencilReadOnly
        };
        m_RenderPass = device.CreateRenderPass(rpDesc);

        // Shaders
        auto vertexShader   = device.CreateShader("/Shaders/Shaders/ForwardPass/Base.vert.spv", RHI::ShaderStage::Vertex);
        auto fragmentShader = device.CreateShader("/Shaders/Shaders/ForwardPass/Base.frag.spv", RHI::ShaderStage::Fragment);

        // Pipeline: 3 vertex streams (pos / texcoord / normal), CullFront, depth test read-only
        RHI::GraphicsPipelineDesc pipelineDesc;
        pipelineDesc.m_VertexShader   = vertexShader.get();
        pipelineDesc.m_FragmentShader = fragmentShader.get();

        pipelineDesc.m_VertexBindings = {
            { 0, 3 * sizeof(float), RHI::VertexInputRate::PerVertex },   // positions
            { 1, 2 * sizeof(float), RHI::VertexInputRate::PerVertex },   // texcoords
            { 2, 3 * sizeof(float), RHI::VertexInputRate::PerVertex },   // normals
        };
        pipelineDesc.m_VertexAttributes = {
            { 0, 0, RHI::Format::R32G32B32Float, 0 },   // position
            { 1, 1, RHI::Format::R32G32Float,    0 },   // texcoord
            { 2, 2, RHI::Format::R32G32B32Float, 0 },   // normal
        };

        pipelineDesc.m_Topology         = RHI::PrimitiveTopology::TriangleList;
        pipelineDesc.m_CullMode         = RHI::CullMode::Back;
        pipelineDesc.m_FillMode         = RHI::FillMode::Solid;
        pipelineDesc.m_DepthTestEnable  = true;
        pipelineDesc.m_DepthWriteEnable = false;   // depth pre-pass already wrote
        pipelineDesc.m_DepthCompareOp   = RHI::CompareOp::LessOrEqual;

        // One color blend attachment: blending disabled
        pipelineDesc.m_ColorBlendAttachments.push_back(RHI::ColorBlendAttachment{
            false,
            RHI::BlendFactor::One, RHI::BlendFactor::Zero, RHI::BlendOp::Add,
            RHI::BlendFactor::One, RHI::BlendFactor::Zero, RHI::BlendOp::Add
        });

        pipelineDesc.m_DescriptorSetLayouts = {
            m_FrameLayout.get(),
            &registry.GetMaterialDescriptorSetLayout()
        };
        pipelineDesc.m_PushConstantRanges = {
            { RHI::ShaderStage::Vertex, 0, static_cast<uint32_t>(sizeof(ForwardPassPushConstants)) }
        };
        pipelineDesc.m_RenderPass = m_RenderPass.get();

        m_Pipeline = device.CreateGraphicsPipeline(pipelineDesc);

        // Framebuffer: scene color + depth
        const auto& colorBuffer = resourceManager.GetSceneColorBuffer();
        const auto& depthBuffer = resourceManager.GetRHIDepthBuffer();
        RHI::FramebufferDesc fbDesc;
        fbDesc.m_RenderPass        = m_RenderPass.get();
        fbDesc.m_ColorAttachments  = { &colorBuffer };
        fbDesc.m_DepthAttachment   = &depthBuffer;
        fbDesc.m_Width             = colorBuffer.GetWidth();
        fbDesc.m_Height            = colorBuffer.GetHeight();
        m_FrameBuffer = device.CreateFramebuffer(fbDesc);
    }

    ForwardPass::~ForwardPass()
    {
    }

    void ForwardPass::Render(const FD::FrameData& frame, const ResourceManager& resourceManager,
                              RHI::IRHICommandList& cmd, uint32_t frameIndex)
    {
        // Update frame uniform
        ForwardPassFrameUniform ubo{};
        ubo.m_View       = frame.m_Camera.m_View;
        ubo.m_ViewProj   = frame.m_Camera.m_Proj * frame.m_Camera.m_View;
        ubo.m_EyePosition = HM::Vector4(frame.m_Camera.m_Position, 1.0f);
        ubo.m_LightCount = frame.m_Lights.size();
        for (size_t i = 0; i < ubo.m_LightCount; ++i)
            ubo.m_Lights[i] = ToGpuLight(frame.m_Lights[i]);
        m_FrameUniforms[frameIndex]->CopyData(&ubo, sizeof(ubo));

        RHI::ClearValue colorClear;
        colorClear.m_Color = { 0.0f, 0.0f, 0.0f, 1.0f };
        RHI::ClearValue depthClear;
        depthClear.m_IsDepth      = true;
        depthClear.m_DepthStencil = { 1.0f, 0 };

        cmd.BeginRenderPass(*m_RenderPass, *m_FrameBuffer, { colorClear, depthClear });

        const uint32_t width  = m_FrameBuffer->GetWidth();
        const uint32_t height = m_FrameBuffer->GetHeight();

        cmd.BindPipeline(*m_Pipeline);
        cmd.SetViewport({ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f });
        cmd.SetScissor({ 0, 0, width, height });

        auto& registry  = resourceManager.GetResourceRegistry();
        auto& posBuffer = const_cast<RHI::IRHIBuffer&>(registry.GetPositionsBuffer());
        auto& uvBuffer  = const_cast<RHI::IRHIBuffer&>(registry.GetTexCoordsBuffer());
        auto& nrmBuffer = const_cast<RHI::IRHIBuffer&>(registry.GetNormalsBuffer());
        auto& idxBuffer = const_cast<RHI::IRHIBuffer&>(registry.GetIndexBuffer());

        cmd.BindVertexBuffers(0, { &posBuffer, &uvBuffer, &nrmBuffer }, { 0, 0, 0 });
        cmd.BindIndexBuffer(idxBuffer, RHI::IndexType::Uint32);

        cmd.BindDescriptorSet(*m_Pipeline, 0, *m_FrameSets[frameIndex]);

        for (const auto& drawNode : frame.m_DrawList.m_Opaque)
        {
            cmd.BindDescriptorSet(
                *m_Pipeline, 1, registry.GetMaterialDescriptorSet(static_cast<uint32_t>(drawNode.m_MaterialIndex)));

            for (const auto& object : drawNode.m_Objects)
            {
                cmd.PushConstants(
                    *m_Pipeline,
                    RHI::ShaderStage::Vertex,
                    0,
                    static_cast<uint32_t>(sizeof(ForwardPassPushConstants)),
                    &object.m_Transform);

                const auto& geom = registry.GetMeshGeometryInfo(object.m_MeshIndex);
                cmd.DrawIndexed(geom.m_IndexCount, 1, geom.m_FirstIndex, geom.m_VertexOffset, 0);
            }
        }

        cmd.EndRenderPass();
    }

    void ForwardPass::Cleanup(RHI::IRHIDevice& device)
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

    void ForwardPass::ResizeResources(RHI::IRHIDevice& device, const ResourceManager& resourceManager)
    {
        const auto& colorBuffer = resourceManager.GetSceneColorBuffer();
        const auto& depthBuffer = resourceManager.GetRHIDepthBuffer();

        m_FrameBuffer.reset();

        RHI::FramebufferDesc fbDesc;
        fbDesc.m_RenderPass       = m_RenderPass.get();
        fbDesc.m_ColorAttachments = { &colorBuffer };
        fbDesc.m_DepthAttachment  = &depthBuffer;
        fbDesc.m_Width            = colorBuffer.GetWidth();
        fbDesc.m_Height           = colorBuffer.GetHeight();
        m_FrameBuffer = device.CreateFramebuffer(fbDesc);
    }

}
