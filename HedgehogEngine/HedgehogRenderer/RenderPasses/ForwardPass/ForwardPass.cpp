#include "ForwardPass.hpp"
#include "ForwardPassPushConstants.hpp"

#include "HedgehogContext/Context/Context.hpp"
#include "HedgehogContext/Context/EngineContext.hpp"
#include "HedgehogContext/Context/ThreadContext.hpp"
#include "HedgehogContext/Context/VulkanContext.hpp"
#include "HedgehogContext/Context/FrameContext.hpp"

#include "HedgehogContext/Containers/MeshContainer/MeshContainer.hpp"
#include "HedgehogContext/Containers/MeshContainer/Mesh.hpp"
#include "HedgehogContext/Containers/MaterialContainer/MaterialContainer.hpp"
#include "HedgehogContext/Containers/DrawListContrainer/DrawListContainer.hpp"
#include "HedgehogContext/Containers/LightContainer/LightContainer.hpp"

#include "HedgehogRenderer/ResourceManager/ResourceManager.hpp"

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

    ForwardPass::ForwardPass(const Context::Context& context, const ResourceManager& resourceManager)
    {
        auto& rhiDevice       = context.GetVulkanContext().GetRHIDevice();
        auto& materialContainer = context.GetEngineContext().GetMaterialContainer();

        // Frame descriptor set layout: binding 0 = UB (vertex + fragment)
        m_FrameLayout = rhiDevice.CreateDescriptorSetLayout({
            { 0, RHI::DescriptorType::UniformBuffer, 1, RHI::ShaderStage::Vertex | RHI::ShaderStage::Fragment }
        });

        m_FramePool = rhiDevice.CreateDescriptorPool(
            MAX_FRAMES_IN_FLIGHT,
            { { RHI::DescriptorType::UniformBuffer, MAX_FRAMES_IN_FLIGHT } });

        m_FrameUniforms.reserve(MAX_FRAMES_IN_FLIGHT);
        m_FrameSets.reserve(MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            auto ubo = rhiDevice.CreateBuffer(
                sizeof(ForwardPassFrameUniform),
                RHI::BufferUsage::UniformBuffer,
                RHI::MemoryUsage::CpuToGpu);

            auto set = rhiDevice.AllocateDescriptorSet(*m_FramePool, *m_FrameLayout);
            set->WriteUniformBuffer(0, *ubo);
            set->Flush();

            m_FrameUniforms.push_back(std::move(ubo));
            m_FrameSets.push_back(std::move(set));
        }

        // Render pass: one color + depth (loaded from DepthPrePass)
        RHI::RenderPassDesc rpDesc;
        rpDesc.m_ColorAttachments.push_back(RHI::AttachmentDesc{
            resourceManager.GetRHIColorBuffer().GetFormat(),
            RHI::LoadOp::Clear,
            RHI::StoreOp::Store,
            RHI::LoadOp::DontCare,
            RHI::StoreOp::DontCare,
            RHI::ImageLayout::Undefined,
            RHI::ImageLayout::Present   // matches original VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
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
        m_RenderPass = rhiDevice.CreateRenderPass(rpDesc);

        // Shaders
        auto vertexShader   = rhiDevice.CreateShader("/Shaders/Shaders/ForwardPass/Base.vert.spv", RHI::ShaderStage::Vertex);
        auto fragmentShader = rhiDevice.CreateShader("/Shaders/Shaders/ForwardPass/Base.frag.spv", RHI::ShaderStage::Fragment);

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
            &materialContainer.GetRHIDescriptorSetLayout()
        };
        pipelineDesc.m_PushConstantRanges = {
            { RHI::ShaderStage::Vertex, 0, static_cast<uint32_t>(sizeof(ForwardPassPushConstants)) }
        };
        pipelineDesc.m_RenderPass = m_RenderPass.get();

        m_Pipeline = rhiDevice.CreateGraphicsPipeline(pipelineDesc);

        // Framebuffer: color + depth
        const auto& colorBuffer = resourceManager.GetRHIColorBuffer();
        const auto& depthBuffer = resourceManager.GetRHIDepthBuffer();
        RHI::FramebufferDesc fbDesc;
        fbDesc.m_RenderPass        = m_RenderPass.get();
        fbDesc.m_ColorAttachments  = { &colorBuffer };
        fbDesc.m_DepthAttachment   = &depthBuffer;
        fbDesc.m_Width             = colorBuffer.GetWidth();
        fbDesc.m_Height            = colorBuffer.GetHeight();
        m_FrameBuffer = rhiDevice.CreateFramebuffer(fbDesc);
    }

    ForwardPass::~ForwardPass()
    {
    }

    void ForwardPass::Render(Context::Context& context, const ResourceManager& resourceManager)
    {
        auto& frameContext  = context.GetFrameContext();
        auto& threadContext = context.GetThreadContext();
        auto& engineContext = context.GetEngineContext();

        const uint32_t frameIndex = threadContext.GetFrameIndex();

        auto& materialContainer = engineContext.GetMaterialContainer();
        auto& drawListContainer = engineContext.GetDrawListContainer();
        auto& lightContainer    = engineContext.GetLightContainer();

        // Update frame uniform
        ForwardPassFrameUniform ubo{};
        ubo.m_View       = frameContext.GetCameraViewMatrix();
        ubo.m_ViewProj   = frameContext.GetCameraProjMatrix() * frameContext.GetCameraViewMatrix();
        ubo.m_EyePosition = HM::Vector4(frameContext.GetCameraPosition(), 1.0f);
        ubo.m_LightCount = lightContainer.GetLightCount();
        const auto& lights = lightContainer.GetLights();
        for (size_t i = 0; i < ubo.m_LightCount; ++i)
            ubo.m_Lights[i] = lights[i];
        m_FrameUniforms[frameIndex]->CopyData(&ubo, sizeof(ubo));

        auto& commandList = threadContext.GetCommandList();

        RHI::ClearValue colorClear;
        colorClear.m_Color = { 0.0f, 0.0f, 0.0f, 1.0f };
        RHI::ClearValue depthClear;
        depthClear.m_IsDepth      = true;
        depthClear.m_DepthStencil = { 1.0f, 0 };

        commandList.BeginRenderPass(*m_RenderPass, *m_FrameBuffer, { colorClear, depthClear });

        const uint32_t width  = m_FrameBuffer->GetWidth();
        const uint32_t height = m_FrameBuffer->GetHeight();

        commandList.BindPipeline(*m_Pipeline);
        commandList.SetViewport({ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f });
        commandList.SetScissor({ 0, 0, width, height });

        auto& meshContainer = engineContext.GetMeshContainer();
        auto& posBuffer = const_cast<RHI::IRHIBuffer&>(meshContainer.GetRHIPositionsBuffer());
        auto& uvBuffer  = const_cast<RHI::IRHIBuffer&>(meshContainer.GetRHITexCoordsBuffer());
        auto& nrmBuffer = const_cast<RHI::IRHIBuffer&>(meshContainer.GetRHINormalsBuffer());
        auto& idxBuffer = const_cast<RHI::IRHIBuffer&>(meshContainer.GetRHIIndexBuffer());

        commandList.BindVertexBuffers(0, { &posBuffer, &uvBuffer, &nrmBuffer }, { 0, 0, 0 });
        commandList.BindIndexBuffer(idxBuffer, RHI::IndexType::Uint32);

        commandList.BindDescriptorSet(*m_Pipeline, 0, *m_FrameSets[frameIndex]);

        for (const auto& drawNode : drawListContainer.GetOpaqueList())
        {
            commandList.BindDescriptorSet(
                *m_Pipeline, 1, materialContainer.GetRHIDescriptorSet(drawNode.materialIndex));

            for (const auto& object : drawNode.objects)
            {
                commandList.PushConstants(
                    *m_Pipeline,
                    RHI::ShaderStage::Vertex,
                    0,
                    static_cast<uint32_t>(sizeof(ForwardPassPushConstants)),
                    &object.objMatrix);

                const auto& mesh = meshContainer.GetMesh(object.meshIndex);
                commandList.DrawIndexed(mesh.GetIndexCount(), 1, mesh.GetFirstIndex(), mesh.GetVertexOffset(), 0);
            }
        }

        commandList.EndRenderPass();
    }

    void ForwardPass::Cleanup(const Context::Context& context)
    {
        context.GetVulkanContext().GetRHIDevice().WaitIdle();

        m_FrameSets.clear();
        m_FrameUniforms.clear();
        m_Pipeline.reset();
        m_FrameBuffer.reset();
        m_RenderPass.reset();
        m_FramePool.reset();
        m_FrameLayout.reset();
    }

    void ForwardPass::ResizeResources(const Context::Context& context, const ResourceManager& resourceManager)
    {
        auto& rhiDevice     = context.GetVulkanContext().GetRHIDevice();
        const auto& colorBuffer = resourceManager.GetRHIColorBuffer();
        const auto& depthBuffer = resourceManager.GetRHIDepthBuffer();

        m_FrameBuffer.reset();

        RHI::FramebufferDesc fbDesc;
        fbDesc.m_RenderPass       = m_RenderPass.get();
        fbDesc.m_ColorAttachments = { &colorBuffer };
        fbDesc.m_DepthAttachment  = &depthBuffer;
        fbDesc.m_Width            = colorBuffer.GetWidth();
        fbDesc.m_Height           = colorBuffer.GetHeight();
        m_FrameBuffer = rhiDevice.CreateFramebuffer(fbDesc);
    }

}
