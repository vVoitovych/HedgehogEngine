#include "ShadowmapPass.hpp"
#include "ShadowmapPassPushConstants.hpp"

#include "HedgehogContext/Context/Context.hpp"
#include "HedgehogContext/Context/EngineContext.hpp"
#include "HedgehogContext/Context/ThreadContext.hpp"
#include "HedgehogContext/Context/VulkanContext.hpp"

#include "HedgehogCommon/api/Camera.hpp"
#include "HedgehogCommon/api/RendererSettings.hpp"

#include "HedgehogSettings/Settings/HedgehogSettings.hpp"
#include "HedgehogSettings/Settings/ShadowmapingSettings.hpp"

#include "HedgehogRenderer/ResourceManager/ResourceManager.hpp"

#include "HedgehogContext/Containers/MeshContainer/MeshContainer.hpp"
#include "HedgehogContext/Containers/MeshContainer/Mesh.hpp"
#include "HedgehogContext/Containers/DrawListContrainer/DrawListContainer.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHIRenderPass.hpp"
#include "RHI/api/IRHIFramebuffer.hpp"
#include "RHI/api/IRHIPipeline.hpp"
#include "RHI/api/IRHIDescriptor.hpp"
#include "RHI/api/IRHIBuffer.hpp"
#include "RHI/api/IRHITexture.hpp"
#include "RHI/api/IRHIShader.hpp"

#include "Scene/Scene.hpp"

#include <algorithm>
#include <cmath>

namespace Renderer
{

    ShadowmapPass::ShadowmapPass(const Context::Context& context, const ResourceManager& resourceManager)
    {
        auto& rhiDevice = context.GetVulkanContext().GetRHIDevice();

        // Descriptor set layout: binding 0 = uniform buffer (vertex stage)
        m_ShadowmapLayout = rhiDevice.CreateDescriptorSetLayout({
            { 0, RHI::DescriptorType::UniformBuffer, 1, RHI::ShaderStage::Vertex }
        });

        // Pool: one UB per cascade per frame
        const uint32_t totalSets = MaxShadowCascades * MAX_FRAMES_IN_FLIGHT;
        m_ShadowmapPool = rhiDevice.CreateDescriptorPool(
            totalSets,
            { { RHI::DescriptorType::UniformBuffer, totalSets } });

        // Per-frame per-cascade uniform buffers and descriptor sets
        m_ShadowmapUniforms.resize(MAX_FRAMES_IN_FLIGHT);
        m_ShadowmapSets.resize(MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            for (size_t j = 0; j < MaxShadowCascades; ++j)
            {
                auto ubo = rhiDevice.CreateBuffer(
                    sizeof(ShadowCascadeUniform),
                    RHI::BufferUsage::UniformBuffer,
                    RHI::MemoryUsage::CpuToGpu);

                auto set = rhiDevice.AllocateDescriptorSet(*m_ShadowmapPool, *m_ShadowmapLayout);
                set->WriteUniformBuffer(0, *ubo);
                set->Flush();

                m_ShadowmapUniforms[i].push_back(std::move(ubo));
                m_ShadowmapSets[i].push_back(std::move(set));
            }
        }

        // Render pass: depth-only, Clear/DontCare, Undefined → DepthStencilReadOnly
        RHI::RenderPassDesc rpDesc;
        rpDesc.m_DepthAttachment = RHI::AttachmentDesc{
            resourceManager.GetRHIShadowMap().GetFormat(),
            RHI::LoadOp::Clear,
            RHI::StoreOp::DontCare,
            RHI::LoadOp::DontCare,
            RHI::StoreOp::DontCare,
            RHI::ImageLayout::Undefined,
            RHI::ImageLayout::DepthStencilReadOnly
        };
        m_RenderPass = rhiDevice.CreateRenderPass(rpDesc);

        // Vertex shader (depth/shadow pass — no fragment shader)
        auto vertexShader = rhiDevice.CreateShader(
            "/Shaders/Shaders/ShadowmapPass/Shadowmap.vert.spv",
            RHI::ShaderStage::Vertex);

        // Pipeline: positions only, CullBack, DepthCompare LessOrEqual
        RHI::GraphicsPipelineDesc pipelineDesc;
        pipelineDesc.m_VertexShader   = vertexShader.get();
        pipelineDesc.m_FragmentShader = nullptr;

        pipelineDesc.m_VertexBindings   = { { 0, 3 * sizeof(float), RHI::VertexInputRate::PerVertex } };
        pipelineDesc.m_VertexAttributes = { { 0, 0, RHI::Format::R32G32B32Float, 0 } };

        pipelineDesc.m_Topology         = RHI::PrimitiveTopology::TriangleList;
        pipelineDesc.m_CullMode         = RHI::CullMode::Back;
        pipelineDesc.m_FillMode         = RHI::FillMode::Solid;
        pipelineDesc.m_DepthTestEnable  = true;
        pipelineDesc.m_DepthWriteEnable = true;
        pipelineDesc.m_DepthCompareOp   = RHI::CompareOp::LessOrEqual;

        pipelineDesc.m_DescriptorSetLayouts = { m_ShadowmapLayout.get() };
        pipelineDesc.m_PushConstantRanges   = {
            { RHI::ShaderStage::Vertex, 0, static_cast<uint32_t>(sizeof(ShadowmapPassPushConstants)) }
        };
        pipelineDesc.m_RenderPass = m_RenderPass.get();

        m_Pipeline = rhiDevice.CreateGraphicsPipeline(pipelineDesc);

        UpdateFrameBuffer(context, resourceManager);
        UpdateViewports(context);
    }

    ShadowmapPass::~ShadowmapPass()
    {
    }

    void ShadowmapPass::Render(Context::Context& context, const ResourceManager& resourceManager)
    {
        auto& threadContext = context.GetThreadContext();
        auto& engineContext = context.GetEngineContext();
        auto& settings      = engineContext.GetSettings().GetShadowmapSettings();

        const uint32_t frameIndex    = threadContext.GetFrameIndex();
        const uint32_t cascadesCount = settings->GetCascadesCount();
        const uint32_t shadowmapSize = settings->GetShadowmapSize();

        auto& commandList = threadContext.GetCommandList();

        RHI::ClearValue depthClear;
        depthClear.m_IsDepth      = true;
        depthClear.m_DepthStencil = { 1.0f, 0 };

        commandList.BeginRenderPass(*m_RenderPass, *m_FrameBuffer, { depthClear });
        commandList.BindPipeline(*m_Pipeline);

        auto& meshContainer = engineContext.GetMeshContainer();
        auto& posBuffer = const_cast<RHI::IRHIBuffer&>(meshContainer.GetRHIPositionsBuffer());
        auto& idxBuffer = const_cast<RHI::IRHIBuffer&>(meshContainer.GetRHIIndexBuffer());

        commandList.BindVertexBuffers(0, { &posBuffer }, { 0 });
        commandList.BindIndexBuffer(idxBuffer, RHI::IndexType::Uint32);

        auto& drawListContainer = engineContext.GetDrawListContainer();

        for (size_t i = 0; i < cascadesCount; ++i)
        {
            const auto& view = m_ShadowViewports[cascadesCount - 1][i];
            commandList.SetViewport({ view.m_X, view.m_Y, view.m_Width, view.m_Height, 0.0f, 1.0f });
            commandList.SetScissor({ 0, 0, shadowmapSize, shadowmapSize });

            commandList.BindDescriptorSet(*m_Pipeline, 0, *m_ShadowmapSets[frameIndex][i]);

            for (const auto& drawNode : drawListContainer.GetOpaqueList())
            {
                for (const auto& object : drawNode.objects)
                {
                    commandList.PushConstants(
                        *m_Pipeline,
                        RHI::ShaderStage::Vertex,
                        0,
                        static_cast<uint32_t>(sizeof(ShadowmapPassPushConstants)),
                        &object.objMatrix);

                    const auto& mesh = meshContainer.GetMesh(object.meshIndex);
                    commandList.DrawIndexed(mesh.GetIndexCount(), 1, mesh.GetFirstIndex(), mesh.GetVertexOffset(), 0);
                }
            }
        }

        commandList.EndRenderPass();
    }

    void ShadowmapPass::Cleanup(const Context::Context& context)
    {
        context.GetVulkanContext().GetRHIDevice().WaitIdle();

        m_ShadowmapSets.clear();
        m_ShadowmapUniforms.clear();
        m_Pipeline.reset();
        m_FrameBuffer.reset();
        m_RenderPass.reset();
        m_ShadowmapPool.reset();
        m_ShadowmapLayout.reset();
    }

    void ShadowmapPass::UpdateData(const Context::Context& context)
    {
        UpdateShadowmapMatrices(context);

        const auto& settings   = context.GetEngineContext().GetSettings().GetShadowmapSettings();
        const uint32_t frame   = context.GetThreadContext().GetFrameIndex();
        const uint32_t cascades = settings->GetCascadesCount();

        for (size_t i = 0; i < cascades; ++i)
        {
            ShadowCascadeUniform ubo;
            ubo.m_ShadowMatrix = m_ShadowmapMatrices[i];
            m_ShadowmapUniforms[frame][i]->CopyData(&ubo, sizeof(ubo));
        }
    }

    void ShadowmapPass::UpdateResources(const Context::Context& context, const ResourceManager& resourceManager)
    {
        if (!context.GetEngineContext().GetSettings().GetShadowmapSettings()->IsDirty())
            return;

        UpdateFrameBuffer(context, resourceManager);
        UpdateViewports(context);
    }

    void ShadowmapPass::UpdateFrameBuffer(const Context::Context& context, const ResourceManager& resourceManager)
    {
        auto& rhiDevice = context.GetVulkanContext().GetRHIDevice();
        const auto& shadowMap = resourceManager.GetRHIShadowMap();

        m_FrameBuffer.reset();

        RHI::FramebufferDesc fbDesc;
        fbDesc.m_RenderPass      = m_RenderPass.get();
        fbDesc.m_DepthAttachment = &shadowMap;
        fbDesc.m_Width           = shadowMap.GetWidth();
        fbDesc.m_Height          = shadowMap.GetHeight();
        m_FrameBuffer = rhiDevice.CreateFramebuffer(fbDesc);
    }

    void ShadowmapPass::UpdateViewports(const Context::Context& context)
    {
        const auto& settings = context.GetEngineContext().GetSettings().GetShadowmapSettings();
        const float sz = static_cast<float>(settings->GetShadowmapSize());

        m_ShadowViewports.clear();
        m_ShadowViewports.resize(MaxShadowCascades);

        m_ShadowViewports[0].push_back({ 0.0f, 0.0f, sz, sz });

        m_ShadowViewports[1].push_back({ 0.0f,    0.0f, sz / 2.0f, sz });
        m_ShadowViewports[1].push_back({ sz / 2.0f, 0.0f, sz / 2.0f, sz });

        m_ShadowViewports[2].push_back({ 0.0f,    0.0f,    sz / 2.0f, sz });
        m_ShadowViewports[2].push_back({ sz / 2.0f, 0.0f,    sz / 2.0f, sz / 2.0f });
        m_ShadowViewports[2].push_back({ sz / 2.0f, sz / 2.0f, sz / 2.0f, sz / 2.0f });

        m_ShadowViewports[3].push_back({ 0.0f,    0.0f,    sz / 2.0f, sz / 2.0f });
        m_ShadowViewports[3].push_back({ 0.0f,    sz / 2.0f, sz / 2.0f, sz / 2.0f });
        m_ShadowViewports[3].push_back({ sz / 2.0f, 0.0f,    sz / 2.0f, sz / 2.0f });
        m_ShadowViewports[3].push_back({ sz / 2.0f, sz / 2.0f, sz / 2.0f, sz / 2.0f });
    }

    void ShadowmapPass::UpdateShadowmapMatrices(const Context::Context& context)
    {
        const auto& settings = context.GetEngineContext().GetSettings().GetShadowmapSettings();
        std::vector<float> cascadeSplits;

        const auto& camera = context.GetEngineContext().GetCamera();
        const float nearClip  = camera.GetNearPlane();
        const float farClip   = camera.GetFarPlane();
        const float clipRange = farClip - nearClip;

        const float minZ  = nearClip;
        const float maxZ  = nearClip + clipRange;
        const float range = maxZ - minZ;
        const float ratio = maxZ / minZ;

        const uint32_t cascadesCount      = settings->GetCascadesCount();
        const float    cascadeSplitLambda = settings->GetCascadeSplitLambda();

        for (uint32_t i = 0; i < cascadesCount; ++i)
        {
            const float p       = (i + 1) / static_cast<float>(cascadesCount);
            const float log     = minZ * std::pow(ratio, p);
            const float uniform = minZ + range * p;
            const float d       = cascadeSplitLambda * (log - uniform) + uniform;
            cascadeSplits.push_back((d - nearClip) / clipRange);
        }

        float lastSplitDist = 0.0f;
        HM::Matrix4x4 camMatrix = camera.GetViewMatrix() * camera.GetProjectionMatrix();
        bool success = true;
        HM::Matrix4x4 invCam = camMatrix.Inverse(success);

        for (uint32_t i = 0; i < cascadesCount; ++i)
        {
            const float splitDist = cascadeSplits[i];

            HM::Vector3 frustumCorners[8] =
            {
                HM::Vector3(-1.0f,  1.0f, 0.0f),
                HM::Vector3( 1.0f,  1.0f, 0.0f),
                HM::Vector3( 1.0f, -1.0f, 0.0f),
                HM::Vector3(-1.0f, -1.0f, 0.0f),
                HM::Vector3(-1.0f,  1.0f, 1.0f),
                HM::Vector3( 1.0f,  1.0f, 1.0f),
                HM::Vector3( 1.0f, -1.0f, 1.0f),
                HM::Vector3(-1.0f, -1.0f, 1.0f),
            };

            for (uint32_t j = 0; j < 8; ++j)
            {
                HM::Vector4 invCorner = invCam * HM::Vector4(frustumCorners[j], 1.0f);
                frustumCorners[j] = invCorner / invCorner.w();
            }

            for (uint32_t j = 0; j < 4; ++j)
            {
                HM::Vector3 dist = frustumCorners[j + 4] - frustumCorners[j];
                frustumCorners[j + 4] = frustumCorners[j] + (dist * splitDist);
                frustumCorners[j]     = frustumCorners[j] + (dist * lastSplitDist);
            }

            HM::Vector3 frustumCenter = HM::Vector3(0.0f, 0.0f, 0.0f);
            for (uint32_t j = 0; j < 8; ++j)
                frustumCenter += frustumCorners[j];
            frustumCenter /= 8.0f;

            float radius = 0.0f;
            for (uint32_t j = 0; j < 8; ++j)
            {
                float distance = (frustumCorners[j] - frustumCenter).Length3Slow();
                radius = std::max(radius, distance);
            }
            radius = std::ceil(radius * 16.0f) / 16.0f;

            const HM::Vector3 maxExtents = HM::Vector3({ radius, radius, radius });
            const HM::Vector3 minExtents = -maxExtents;

            HM::Vector3 lightDir = HM::Vector3(1.0f, 0.0f, 0.0f);
            const auto& shadowDir = context.GetEngineContext().GetScene().GetShadowLightDirection();
            if (shadowDir.has_value())
                lightDir = shadowDir.value();

            const HM::Matrix4x4 lightViewMatrix  = HM::Matrix4x4::LookAt(
                frustumCenter - lightDir * radius, frustumCenter, HM::Vector3(0.0f, 0.0f, 1.0f));
            const HM::Matrix4x4 lightOrthoMatrix = HM::Matrix4x4::Ortho(
                minExtents.x(), maxExtents.x(), minExtents.y(), maxExtents.y(),
                0.0f, maxExtents.z() - minExtents.z());

            m_ShadowmapMatrices[i] = lightOrthoMatrix * lightViewMatrix;

            lastSplitDist = cascadeSplits[i];
        }
    }

}
