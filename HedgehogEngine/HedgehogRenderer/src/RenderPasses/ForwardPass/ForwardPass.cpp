#include "ForwardPass.hpp"
#include "ForwardPassPushConstants.hpp"

#include "HedgehogEngine/api/Frame/FrameData.hpp"

#include "ResourceManager/ResourceManager.hpp"
#include "ResourceRegistry/ResourceRegistry.hpp"
#include "ResourceRegistry/MeshGpuData.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"
#include "HedgehogMath/api/Common.hpp"

#include "ShaderManager/ShaderManager.hpp"
#include "PipelineManager/PipelineManager.hpp"

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
    ForwardPass::GpuLight ForwardPass::ToGpuLight(const HedgehogEngine::LightData& fd)
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


    ForwardPass::ForwardPass(RHI::IRHIDevice& device, ResourceManager& resourceManager,
                             ShaderManager& shaderManager, PipelineManager& pipelineManager)
    {
        const auto sd = shaderManager.LoadShaderFile(
            "/HedgehogEngine/HedgehogRenderer/assets/Shaders/ForwardPass.shader");
        assert(sd.m_Layout.m_DescriptorSets.size() >= 2);

        // Set 0: per-frame data (camera, lights)
        m_FrameLayout = device.CreateDescriptorSetLayout(sd.m_Layout.m_DescriptorSets[0]);
        m_FramePool = device.CreateDescriptorPool(
            MAX_FRAMES_IN_FLIGHT,
            PipelineManager::MakePoolSizes(sd.m_Layout.m_DescriptorSets[0], MAX_FRAMES_IN_FLIGHT));

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

        // Set 1: per-material data — ForwardPass defines and owns this layout.
        // The layout is injected into ResourceRegistry so it can allocate material descriptor sets.
        m_MaterialLayout = device.CreateDescriptorSetLayout(sd.m_Layout.m_DescriptorSets[1]);
        resourceManager.GetResourceRegistry().SetMaterialLayout(
            device,
            *m_MaterialLayout,
            MAX_MATERIAL_COUNT,
            PipelineManager::MakePoolSizes(sd.m_Layout.m_DescriptorSets[1], MAX_MATERIAL_COUNT));

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

        // Pipeline
        auto pipelineDesc                   = sd.m_Pipeline;
        pipelineDesc.m_DescriptorSetLayouts = { m_FrameLayout.get(), m_MaterialLayout.get() };
        pipelineDesc.m_RenderPass           = m_RenderPass.get();
        m_Pipeline = &pipelineManager.GetOrCreate(pipelineDesc);

        // Framebuffer
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

    void ForwardPass::Render(const HedgehogEngine::FrameData& frame, const ResourceManager& resourceManager,
                              RHI::IRHICommandList& cmd, uint32_t frameIndex)
    {
        ForwardPassFrameUniform ubo{};
        ubo.m_View        = frame.m_Camera.m_View;
        ubo.m_ViewProj    = frame.m_Camera.m_Proj * frame.m_Camera.m_View;
        ubo.m_EyePosition = HM::Vector4(frame.m_Camera.m_Position, 1.0f);
        ubo.m_LightCount  = frame.m_Lights.size();
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
        m_Pipeline = nullptr;
        m_FrameBuffer.reset();
        m_RenderPass.reset();
        m_FramePool.reset();
        m_FrameLayout.reset();
        m_MaterialLayout.reset();
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
