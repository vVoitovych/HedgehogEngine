#include "DepthPrePass.hpp"
#include "DepthPrePassPushConstants.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "HedgehogCommon/api/Frame/FrameData.hpp"

#include "ResourceManager/ResourceManager.hpp"
#include "ResourceRegistry/ResourceRegistry.hpp"
#include "ResourceRegistry/MeshGpuData.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"

#include "Pipeline/ShaderLoader.hpp"
#include "Pipeline/PipelineLoader.hpp"

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

    DepthPrePass::DepthPrePass(RHI::IRHIDevice& device, const ResourceManager& resourceManager,
                                const FS::FileSystemManager& fileSystem)
    {
        const auto sd = ShaderLoader::Load(device,
            "engine://HedgehogEngine/HedgehogRenderer/assets/Shaders/DepthPrepass.shader",
            fileSystem);
        assert(!sd.Layout.DescriptorSets.empty());

        constexpr uint32_t setCount = HedgehogEngine::MAX_FRAMES_IN_FLIGHT * kRenderTargetCount;

        m_FrameLayout = device.CreateDescriptorSetLayout(sd.Layout.DescriptorSets[0]);
        m_FramePool = device.CreateDescriptorPool(
            setCount,
            PipelineLoader::MakePoolSizes(sd.Layout.DescriptorSets[0], setCount));

        // Per-target × per-frame uniform buffers and descriptor sets.
        for (uint32_t t = 0; t < kRenderTargetCount; ++t)
        {
            m_FrameUniforms[t].reserve(HedgehogEngine::MAX_FRAMES_IN_FLIGHT);
            m_FrameSets[t].reserve(HedgehogEngine::MAX_FRAMES_IN_FLIGHT);
            for (size_t i = 0; i < HedgehogEngine::MAX_FRAMES_IN_FLIGHT; ++i)
            {
                auto ubo = device.CreateBuffer(
                    sizeof(DepthPrepassFrameUniform),
                    RHI::BufferUsage::UniformBuffer,
                    RHI::MemoryUsage::CpuToGpu);

                auto set = device.AllocateDescriptorSet(*m_FramePool, *m_FrameLayout);
                set->WriteUniformBuffer(0, *ubo);
                set->Flush();

                m_FrameUniforms[t].push_back(std::move(ubo));
                m_FrameSets[t].push_back(std::move(set));
            }
        }

        // Render pass: depth-only (no color attachments). Shared by both targets (same format).
        RHI::RenderPassDesc rpDesc;
        rpDesc.DepthAttachment = RHI::AttachmentDesc{
            resourceManager.GetRHIDepthBuffer().GetFormat(),
            RHI::LoadOp::Clear,
            RHI::StoreOp::Store,
            RHI::LoadOp::DontCare,
            RHI::StoreOp::DontCare,
            RHI::ImageLayout::Undefined,
            RHI::ImageLayout::DepthStencilReadOnly
        };
        m_RenderPass = device.CreateRenderPass(rpDesc);

        // Pipeline
        auto pipelineDesc                 = sd.Pipeline;
        pipelineDesc.DescriptorSetLayouts = { m_FrameLayout.get() };
        pipelineDesc.RenderPass           = m_RenderPass.get();
        m_Pipeline = device.CreateGraphicsPipeline(pipelineDesc);

        CreateFramebuffer(RenderTargetId::Scene, device, resourceManager);
        CreateFramebuffer(RenderTargetId::Game,  device, resourceManager);
    }

    DepthPrePass::~DepthPrePass()
    {
    }

    void DepthPrePass::CreateFramebuffer(RenderTargetId target, RHI::IRHIDevice& device,
                                          const ResourceManager& resourceManager)
    {
        const auto& depthBuffer = resourceManager.GetDepthBuffer(target);

        RHI::FramebufferDesc fbDesc;
        fbDesc.RenderPass      = m_RenderPass.get();
        fbDesc.DepthAttachment = &depthBuffer;
        fbDesc.Width           = depthBuffer.GetWidth();
        fbDesc.Height          = depthBuffer.GetHeight();
        m_FrameBuffers[static_cast<uint32_t>(target)] = device.CreateFramebuffer(fbDesc);
    }

    void DepthPrePass::Render(RenderTargetId target,
                               const HedgehogEngine::CameraData& camera,
                               const HedgehogEngine::DrawBucket&  opaque,
                               const ResourceManager& resourceManager,
                               RHI::IRHICommandList& cmd, uint32_t frameIndex)
    {
        const uint32_t ti = static_cast<uint32_t>(target);

        DepthPrepassFrameUniform ubo{};
        ubo.ViewProj = camera.Proj * camera.View;
        m_FrameUniforms[ti][frameIndex]->CopyData(&ubo, sizeof(ubo));

        RHI::ClearValue depthClear;
        depthClear.IsDepth      = true;
        depthClear.DepthStencil = { 1.0f, 0 };

        cmd.BeginRenderPass(*m_RenderPass, *m_FrameBuffers[ti], { depthClear });

        const uint32_t width  = m_FrameBuffers[ti]->GetWidth();
        const uint32_t height = m_FrameBuffers[ti]->GetHeight();

        cmd.BindPipeline(*m_Pipeline);
        cmd.SetViewport({ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f });
        cmd.SetScissor({ 0, 0, width, height });

        auto& registry  = resourceManager.GetResourceRegistry();
        auto& posBuffer = const_cast<RHI::IRHIBuffer&>(registry.GetPositionsBuffer());
        cmd.BindVertexBuffers(0, { &posBuffer }, { 0 });

        auto& idxBuffer = const_cast<RHI::IRHIBuffer&>(registry.GetIndexBuffer());
        cmd.BindIndexBuffer(idxBuffer, RHI::IndexType::Uint32);

        cmd.BindDescriptorSet(*m_Pipeline, 0, *m_FrameSets[ti][frameIndex]);

        for (const auto& drawNode : opaque)
        {
            for (const auto& object : drawNode.Objects)
            {
                cmd.PushConstants(
                    *m_Pipeline,
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
    }

    void DepthPrePass::ResizeResources(RenderTargetId target, RHI::IRHIDevice& device,
                                        const ResourceManager& resourceManager)
    {
        m_FrameBuffers[static_cast<uint32_t>(target)].reset();
        CreateFramebuffer(target, device, resourceManager);
    }

}
