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

        constexpr uint32_t setCount = HedgehogEngine::MAX_FRAMES_IN_FLIGHT * kRenderTargetCount;

        // Set 0: per-frame data (camera, lights)
        m_FrameLayout = device.CreateDescriptorSetLayout(sd.Layout.DescriptorSets[0]);
        m_FramePool = device.CreateDescriptorPool(
            setCount,
            PipelineLoader::MakePoolSizes(sd.Layout.DescriptorSets[0], setCount));

        for (uint32_t t = 0; t < kRenderTargetCount; ++t)
        {
            m_FrameUniforms[t].reserve(HedgehogEngine::MAX_FRAMES_IN_FLIGHT);
            m_FrameSets[t].reserve(HedgehogEngine::MAX_FRAMES_IN_FLIGHT);
            for (size_t i = 0; i < HedgehogEngine::MAX_FRAMES_IN_FLIGHT; ++i)
            {
                auto ubo = device.CreateBuffer(
                    sizeof(ForwardPassFrameUniform),
                    RHI::BufferUsage::UniformBuffer,
                    RHI::MemoryUsage::CpuToGpu);

                auto set = device.AllocateDescriptorSet(*m_FramePool, *m_FrameLayout);
                set->WriteUniformBuffer(0, *ubo);
                set->Flush();

                m_FrameUniforms[t].push_back(std::move(ubo));
                m_FrameSets[t].push_back(std::move(set));
            }
        }

        // Set 1: per-material data — ForwardPass defines and owns this layout.
        // The layout is injected into ResourceRegistry so it can allocate material descriptor sets.
        m_MaterialLayout = device.CreateDescriptorSetLayout(sd.Layout.DescriptorSets[1]);
        resourceManager.GetResourceRegistry().SetMaterialLayout(
            device,
            *m_MaterialLayout,
            HedgehogEngine::MAX_MATERIAL_COUNT,
            PipelineLoader::MakePoolSizes(sd.Layout.DescriptorSets[1], HedgehogEngine::MAX_MATERIAL_COUNT));

        // Render pass: one color + depth (loaded from DepthPrepass). Shared by both targets.
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
        auto pipelineDesc                 = sd.Pipeline;
        pipelineDesc.DescriptorSetLayouts = { m_FrameLayout.get(), m_MaterialLayout.get() };
        pipelineDesc.RenderPass           = m_RenderPass.get();
        m_Pipeline = device.CreateGraphicsPipeline(pipelineDesc);

        CreateFramebuffer(RenderTargetId::Scene, device, resourceManager);
        CreateFramebuffer(RenderTargetId::Game,  device, resourceManager);
    }

    ForwardPass::~ForwardPass()
    {
    }

    void ForwardPass::CreateFramebuffer(RenderTargetId target, RHI::IRHIDevice& device,
                                         const ResourceManager& resourceManager)
    {
        const auto& colorBuffer = resourceManager.GetColorBuffer(target);
        const auto& depthBuffer = resourceManager.GetDepthBuffer(target);

        RHI::FramebufferDesc fbDesc;
        fbDesc.RenderPass       = m_RenderPass.get();
        fbDesc.ColorAttachments = { &colorBuffer };
        fbDesc.DepthAttachment  = &depthBuffer;
        fbDesc.Width            = colorBuffer.GetWidth();
        fbDesc.Height           = colorBuffer.GetHeight();
        m_FrameBuffers[static_cast<uint32_t>(target)] = device.CreateFramebuffer(fbDesc);
    }

    void ForwardPass::Render(RenderTargetId target,
                              const HedgehogEngine::CameraData&          camera,
                              const HedgehogEngine::DrawBucket&          opaque,
                              const std::vector<HedgehogEngine::LightData>& lights,
                              const ResourceManager& resourceManager,
                              RHI::IRHICommandList& cmd, uint32_t frameIndex)
    {
        const uint32_t ti = static_cast<uint32_t>(target);

        ForwardPassFrameUniform ubo{};
        ubo.View        = camera.View;
        ubo.ViewProj    = camera.Proj * camera.View;
        ubo.EyePosition = camera.Position;
        ubo.LightCount  = lights.size();
        for (size_t i = 0; i < ubo.LightCount; ++i)
            ubo.Lights[i] = ToGpuLight(lights[i]);
        m_FrameUniforms[ti][frameIndex]->CopyData(&ubo, sizeof(ubo));

        RHI::ClearValue colorClear;
        colorClear.Color = { 0.0f, 0.0f, 0.0f, 1.0f };
        RHI::ClearValue depthClear;
        depthClear.IsDepth      = true;
        depthClear.DepthStencil = { 1.0f, 0 };

        cmd.BeginRenderPass(*m_RenderPass, *m_FrameBuffers[ti], { colorClear, depthClear });

        const uint32_t width  = m_FrameBuffers[ti]->GetWidth();
        const uint32_t height = m_FrameBuffers[ti]->GetHeight();

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

        cmd.BindDescriptorSet(*m_Pipeline, 0, *m_FrameSets[ti][frameIndex]);

        for (const auto& drawNode : opaque)
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

        for (uint32_t t = 0; t < kRenderTargetCount; ++t)
        {
            m_FrameSets[t].clear();
            m_FrameUniforms[t].clear();
            m_FrameBuffers[t].reset();
        }
        m_Pipeline.reset();
        m_RenderPass.reset();
        m_FramePool.reset();
        m_FrameLayout.reset();
        m_MaterialLayout.reset();
    }

    void ForwardPass::ResizeResources(RenderTargetId target, RHI::IRHIDevice& device,
                                       const ResourceManager& resourceManager)
    {
        m_FrameBuffers[static_cast<uint32_t>(target)].reset();
        CreateFramebuffer(target, device, resourceManager);
    }

}
