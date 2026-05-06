#include "ForwardPassNode.hpp"

#include "../RenderContext.hpp"
#include "../../ResourceManager/ResourceManager.hpp"
#include "../../ResourceManager/ResourceNames.hpp"
#include "../../ResourceRegistry/ResourceRegistry.hpp"
#include "../../ResourceRegistry/MeshGpuData.hpp"
#include "../../Pipeline/ShaderLoader.hpp"
#include "../../Pipeline/PipelineLoader.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"
#include "HedgehogEngine/api/Frame/FrameData.hpp"
#include "HedgehogMath/api/Common.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHIRenderPass.hpp"
#include "RHI/api/IRHIFramebuffer.hpp"
#include "RHI/api/IRHIPipeline.hpp"
#include "RHI/api/IRHIDescriptor.hpp"
#include "RHI/api/IRHIBuffer.hpp"
#include "RHI/api/IRHITexture.hpp"

#include <cassert>
#include <cmath>

namespace Renderer
{
    namespace
    {
        struct PushConstants { HM::Matrix4x4 objToWorld; };
    }

    ForwardPassNode::GpuLight ForwardPassNode::ToGpuLight(const HedgehogEngine::LightData& ld)
    {
        GpuLight gpu;
        gpu.m_Position  = ld.m_Position;
        gpu.m_Direction = ld.m_Direction;
        gpu.m_Color     = ld.m_Color;
        gpu.m_Data      = HM::Vector4(
            static_cast<float>(ld.m_Type),
            ld.m_Intensity,
            ld.m_Radius,
            std::cos(HM::ToRadians(ld.m_ConeAngle)));
        return gpu;
    }

    ForwardPassNode::ForwardPassNode(RHI::IRHIDevice& device, ResourceManager& resourceManager)
    {
        const auto sd = ShaderLoader::Load(device,
            "/HedgehogEngine/HedgehogRenderer/Assets/Shaders/ForwardPass.shader");
        assert(sd.m_Layout.m_DescriptorSets.size() >= 2);

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

        m_MaterialLayout = device.CreateDescriptorSetLayout(sd.m_Layout.m_DescriptorSets[1]);
        resourceManager.GetResourceRegistry().SetMaterialLayout(
            device,
            *m_MaterialLayout,
            MAX_MATERIAL_COUNT,
            PipelineLoader::MakePoolSizes(sd.m_Layout.m_DescriptorSets[1], MAX_MATERIAL_COUNT));

        RHI::RenderPassDesc rpDesc;
        rpDesc.m_ColorAttachments.push_back(RHI::AttachmentDesc{
            resourceManager.GetTexture(ResourceNames::SceneColorBuffer).GetFormat(),
            RHI::LoadOp::Clear,    RHI::StoreOp::Store,
            RHI::LoadOp::DontCare, RHI::StoreOp::DontCare,
            RHI::ImageLayout::Undefined, RHI::ImageLayout::ColorAttachment
        });
        rpDesc.m_DepthAttachment = RHI::AttachmentDesc{
            resourceManager.GetTexture(ResourceNames::RHIDepthBuffer).GetFormat(),
            RHI::LoadOp::Load,     RHI::StoreOp::DontCare,
            RHI::LoadOp::DontCare, RHI::StoreOp::DontCare,
            RHI::ImageLayout::DepthStencilReadOnly, RHI::ImageLayout::DepthStencilReadOnly
        };
        m_RenderPass = device.CreateRenderPass(rpDesc);

        auto pipelineDesc                   = sd.m_Pipeline;
        pipelineDesc.m_DescriptorSetLayouts = { m_FrameLayout.get(), m_MaterialLayout.get() };
        pipelineDesc.m_RenderPass           = m_RenderPass.get();
        m_Pipeline = device.CreateGraphicsPipeline(pipelineDesc);

        const auto& colorBuffer = resourceManager.GetTexture(ResourceNames::SceneColorBuffer);
        const auto& depthBuffer = resourceManager.GetTexture(ResourceNames::RHIDepthBuffer);
        RHI::FramebufferDesc fbDesc;
        fbDesc.m_RenderPass       = m_RenderPass.get();
        fbDesc.m_ColorAttachments = { &colorBuffer };
        fbDesc.m_DepthAttachment  = &depthBuffer;
        fbDesc.m_Width            = colorBuffer.GetWidth();
        fbDesc.m_Height           = colorBuffer.GetHeight();
        m_FrameBuffer = device.CreateFramebuffer(fbDesc);

        m_CachedWidth  = colorBuffer.GetWidth();
        m_CachedHeight = colorBuffer.GetHeight();
    }

    ForwardPassNode::~ForwardPassNode() = default;

    void ForwardPassNode::Render(RenderContext& ctx)
    {
        RebuildFramebufferIfNeeded(ctx.m_Device.get(), ctx.m_ResourceManager.get());

        const auto& frame = ctx.m_FrameData.get();
        auto&       cmd   = ctx.m_Cmd.get();
        const auto& rm    = ctx.m_ResourceManager.get();

        FrameUniform ubo{};
        ubo.m_View        = frame.m_Camera.m_View;
        ubo.m_ViewProj    = frame.m_Camera.m_Proj * frame.m_Camera.m_View;
        ubo.m_EyePosition = frame.m_Camera.m_Position;
        ubo.m_LightCount  = frame.m_Lights.size();
        for (size_t i = 0; i < ubo.m_LightCount; ++i)
            ubo.m_Lights[i] = ToGpuLight(frame.m_Lights[i]);
        m_FrameUniforms[ctx.m_FrameIndex]->CopyData(&ubo, sizeof(ubo));

        RHI::ClearValue colorClear;
        colorClear.m_Color = { 0.0f, 0.0f, 0.0f, 1.0f };
        RHI::ClearValue depthClear;
        depthClear.m_IsDepth      = true;
        depthClear.m_DepthStencil = { 1.0f, 0 };

        cmd.BeginRenderPass(*m_RenderPass, *m_FrameBuffer, { colorClear, depthClear });

        const uint32_t w = m_FrameBuffer->GetWidth();
        const uint32_t h = m_FrameBuffer->GetHeight();

        cmd.BindPipeline(*m_Pipeline);
        cmd.SetViewport({ 0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f, 1.0f });
        cmd.SetScissor({ 0, 0, w, h });

        auto& registry  = rm.GetResourceRegistry();
        auto& posBuffer = const_cast<RHI::IRHIBuffer&>(registry.GetPositionsBuffer());
        auto& uvBuffer  = const_cast<RHI::IRHIBuffer&>(registry.GetTexCoordsBuffer());
        auto& nrmBuffer = const_cast<RHI::IRHIBuffer&>(registry.GetNormalsBuffer());
        auto& idxBuffer = const_cast<RHI::IRHIBuffer&>(registry.GetIndexBuffer());

        cmd.BindVertexBuffers(0, { &posBuffer, &uvBuffer, &nrmBuffer }, { 0, 0, 0 });
        cmd.BindIndexBuffer(idxBuffer, RHI::IndexType::Uint32);
        cmd.BindDescriptorSet(*m_Pipeline, 0, *m_FrameSets[ctx.m_FrameIndex]);

        for (const auto& drawNode : frame.m_DrawList.m_Opaque)
        {
            cmd.BindDescriptorSet(
                *m_Pipeline, 1,
                registry.GetMaterialDescriptorSet(static_cast<uint32_t>(drawNode.m_MaterialIndex)));

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

    void ForwardPassNode::Cleanup(RHI::IRHIDevice& device)
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

    void ForwardPassNode::RebuildFramebufferIfNeeded(RHI::IRHIDevice& device, const ResourceManager& rm)
    {
        const auto& colorTex = rm.GetTexture(ResourceNames::SceneColorBuffer);
        if (colorTex.GetWidth() == m_CachedWidth && colorTex.GetHeight() == m_CachedHeight)
            return;

        m_FrameBuffer.reset();

        const auto& depthTex = rm.GetTexture(ResourceNames::RHIDepthBuffer);
        RHI::FramebufferDesc fbDesc;
        fbDesc.m_RenderPass       = m_RenderPass.get();
        fbDesc.m_ColorAttachments = { &colorTex };
        fbDesc.m_DepthAttachment  = &depthTex;
        fbDesc.m_Width            = colorTex.GetWidth();
        fbDesc.m_Height           = colorTex.GetHeight();
        m_FrameBuffer = device.CreateFramebuffer(fbDesc);

        m_CachedWidth  = colorTex.GetWidth();
        m_CachedHeight = colorTex.GetHeight();
    }

} // namespace Renderer
