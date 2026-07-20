#include "ForwardPass.hpp"
#include "ForwardPassPushConstants.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "HedgehogCommon/api/Frame/FrameData.hpp"

#include "ResourceManager/ResourceManager.hpp"
#include "ResourceRegistry/ResourceRegistry.hpp"
#include "ResourceRegistry/MeshGpuData.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"
#include "HedgehogMath/api/Common.hpp"

#include "Pipeline/ShaderLoader.hpp"
#include "Pipeline/PipelineLoader.hpp"

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


    ForwardPass::ForwardPass(RHI::IRHIDevice& device, ResourceManager& resourceManager,
                              const FS::FileSystemManager& fileSystem)
    {
        const auto sd = ShaderLoader::Load(device,
            "engine://HedgehogEngine/HedgehogRenderer/assets/Shaders/ForwardPass.shader",
            fileSystem);
        assert(sd.Layout.DescriptorSets.size() >= 2);

        // Set 0: per-frame data (camera, lights)
        m_FrameLayout = device.CreateDescriptorSetLayout(sd.Layout.DescriptorSets[0]);
        m_FramePool = device.CreateDescriptorPool(
            HedgehogEngine::MAX_FRAMES_IN_FLIGHT,
            PipelineLoader::MakePoolSizes(sd.Layout.DescriptorSets[0], HedgehogEngine::MAX_FRAMES_IN_FLIGHT));

        m_FrameUniforms.reserve(HedgehogEngine::MAX_FRAMES_IN_FLIGHT);
        m_FrameSets.reserve(HedgehogEngine::MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < HedgehogEngine::MAX_FRAMES_IN_FLIGHT; ++i)
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
        m_MaterialLayout = device.CreateDescriptorSetLayout(sd.Layout.DescriptorSets[1]);
        resourceManager.GetResourceRegistry().SetMaterialLayout(
            device,
            *m_MaterialLayout,
            HedgehogEngine::MAX_MATERIAL_COUNT,
            PipelineLoader::MakePoolSizes(sd.Layout.DescriptorSets[1], HedgehogEngine::MAX_MATERIAL_COUNT));

        // Render pass: one color + depth (loaded from DepthPrePass)
        RHI::RenderPassDesc rpDesc;
        rpDesc.ColorAttachments.push_back(RHI::AttachmentDesc{
            resourceManager.GetSceneColorBuffer().GetFormat(),
            RHI::LoadOp::Clear,
            RHI::StoreOp::Store,
            RHI::LoadOp::DontCare,
            RHI::StoreOp::DontCare,
            RHI::ImageLayout::Undefined,
            RHI::ImageLayout::ColorAttachment
        });
        rpDesc.DepthAttachment = RHI::AttachmentDesc{
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
        auto pipelineDesc                   = sd.Pipeline;
        pipelineDesc.DescriptorSetLayouts = { m_FrameLayout.get(), m_MaterialLayout.get() };
        pipelineDesc.RenderPass           = m_RenderPass.get();
        m_Pipeline = device.CreateGraphicsPipeline(pipelineDesc);

        // Framebuffer
        const auto& colorBuffer = resourceManager.GetSceneColorBuffer();
        const auto& depthBuffer = resourceManager.GetRHIDepthBuffer();
        RHI::FramebufferDesc fbDesc;
        fbDesc.RenderPass        = m_RenderPass.get();
        fbDesc.ColorAttachments  = { &colorBuffer };
        fbDesc.DepthAttachment   = &depthBuffer;
        fbDesc.Width             = colorBuffer.GetWidth();
        fbDesc.Height            = colorBuffer.GetHeight();
        m_FrameBuffer = device.CreateFramebuffer(fbDesc);
    }

    ForwardPass::~ForwardPass()
    {
    }

    void ForwardPass::Render(const HedgehogEngine::FrameData& frame, const ResourceManager& resourceManager,
                              RHI::IRHICommandList& cmd, uint32_t frameIndex)
    {
        ForwardPassFrameUniform ubo{};
        ubo.View        = frame.Camera.View;
        ubo.ViewProj    = frame.Camera.Proj * frame.Camera.View;
        ubo.EyePosition = frame.Camera.Position;
        ubo.LightCount  = frame.Lights.size();
        for (size_t i = 0; i < ubo.LightCount; ++i)
            ubo.Lights[i] = ToGpuLight(frame.Lights[i]);
        m_FrameUniforms[frameIndex]->CopyData(&ubo, sizeof(ubo));

        RHI::ClearValue colorClear;
        colorClear.Color = { 0.0f, 0.0f, 0.0f, 1.0f };
        RHI::ClearValue depthClear;
        depthClear.IsDepth      = true;
        depthClear.DepthStencil = { 1.0f, 0 };

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

        for (const auto& drawNode : frame.DrawList.Opaque)
        {
            cmd.BindDescriptorSet(
                *m_Pipeline, 1, registry.GetMaterialDescriptorSet(static_cast<uint32_t>(drawNode.MaterialIndex)));

            for (const auto& object : drawNode.Objects)
            {
                cmd.PushConstants(
                    *m_Pipeline,
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
        m_Pipeline.reset();
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
        fbDesc.RenderPass       = m_RenderPass.get();
        fbDesc.ColorAttachments = { &colorBuffer };
        fbDesc.DepthAttachment  = &depthBuffer;
        fbDesc.Width            = colorBuffer.GetWidth();
        fbDesc.Height           = colorBuffer.GetHeight();
        m_FrameBuffer = device.CreateFramebuffer(fbDesc);
    }

}
