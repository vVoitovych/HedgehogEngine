#include "ShadowmapPass.hpp"
#include "ShadowmapPassPushConstants.hpp"

#include "FrameData/FrameData.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"

#include "HedgehogSettings/Settings/HedgehogSettings.hpp"
#include "HedgehogSettings/Settings/ShadowmapingSettings.hpp"

#include "HedgehogRenderer/ResourceManager/ResourceManager.hpp"
#include "HedgehogRenderer/ResourceRegistry/ResourceRegistry.hpp"
#include "HedgehogRenderer/ResourceRegistry/MeshGpuData.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHIRenderPass.hpp"
#include "RHI/api/IRHIFramebuffer.hpp"
#include "RHI/api/IRHIPipeline.hpp"
#include "RHI/api/IRHIDescriptor.hpp"
#include "RHI/api/IRHIBuffer.hpp"
#include "RHI/api/IRHITexture.hpp"
#include "RHI/api/IRHIShader.hpp"

#include <algorithm>
#include <cmath>

namespace Renderer
{

    ShadowmapPass::ShadowmapPass(RHI::IRHIDevice& device, const HedgehogSettings::Settings& settings,
                                  const ResourceManager& resourceManager)
    {
        // Descriptor set layout: binding 0 = uniform buffer (vertex stage)
        m_ShadowmapLayout = device.CreateDescriptorSetLayout({
            { 0, RHI::DescriptorType::UniformBuffer, 1, RHI::ShaderStage::Vertex }
        });

        // Pool: one UB per cascade per frame
        const uint32_t totalSets = MaxShadowCascades * MAX_FRAMES_IN_FLIGHT;
        m_ShadowmapPool = device.CreateDescriptorPool(
            totalSets,
            { { RHI::DescriptorType::UniformBuffer, totalSets } });

        // Per-frame per-cascade uniform buffers and descriptor sets
        m_ShadowmapUniforms.resize(MAX_FRAMES_IN_FLIGHT);
        m_ShadowmapSets.resize(MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            for (size_t j = 0; j < MaxShadowCascades; ++j)
            {
                auto ubo = device.CreateBuffer(
                    sizeof(ShadowCascadeUniform),
                    RHI::BufferUsage::UniformBuffer,
                    RHI::MemoryUsage::CpuToGpu);

                auto set = device.AllocateDescriptorSet(*m_ShadowmapPool, *m_ShadowmapLayout);
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
        m_RenderPass = device.CreateRenderPass(rpDesc);

        // Vertex shader (depth/shadow pass — no fragment shader)
        auto vertexShader = device.CreateShader(
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

        m_Pipeline = device.CreateGraphicsPipeline(pipelineDesc);

        UpdateFrameBuffer(device, resourceManager);
        UpdateViewports(settings);
    }

    ShadowmapPass::~ShadowmapPass()
    {
    }

    void ShadowmapPass::Render(const FD::FrameData& frame, const ResourceManager& resourceManager,
                                RHI::IRHICommandList& cmd, uint32_t frameIndex)
    {
        RHI::ClearValue depthClear;
        depthClear.m_IsDepth      = true;
        depthClear.m_DepthStencil = { 1.0f, 0 };

        cmd.BeginRenderPass(*m_RenderPass, *m_FrameBuffer, { depthClear });
        cmd.BindPipeline(*m_Pipeline);

        auto& registry  = resourceManager.GetResourceRegistry();
        auto& posBuffer = const_cast<RHI::IRHIBuffer&>(registry.GetPositionsBuffer());
        auto& idxBuffer = const_cast<RHI::IRHIBuffer&>(registry.GetIndexBuffer());

        cmd.BindVertexBuffers(0, { &posBuffer }, { 0 });
        cmd.BindIndexBuffer(idxBuffer, RHI::IndexType::Uint32);

        for (size_t i = 0; i < m_CascadesCount; ++i)
        {
            const auto& view = m_ShadowViewports[m_CascadesCount - 1][i];
            cmd.SetViewport({ view.m_X, view.m_Y, view.m_Width, view.m_Height, 0.0f, 1.0f });
            cmd.SetScissor({ 0, 0, m_ShadowmapSize, m_ShadowmapSize });

            cmd.BindDescriptorSet(*m_Pipeline, 0, *m_ShadowmapSets[frameIndex][i]);

            for (const auto& drawNode : frame.m_DrawList.m_Opaque)
            {
                for (const auto& object : drawNode.m_Objects)
                {
                    cmd.PushConstants(
                        *m_Pipeline,
                        RHI::ShaderStage::Vertex,
                        0,
                        static_cast<uint32_t>(sizeof(ShadowmapPassPushConstants)),
                        &object.m_Transform);

                    const auto& geom = registry.GetMeshGeometryInfo(object.m_MeshIndex);
                    cmd.DrawIndexed(geom.m_IndexCount, 1, geom.m_FirstIndex, geom.m_VertexOffset, 0);
                }
            }
        }

        cmd.EndRenderPass();
    }

    void ShadowmapPass::Cleanup(RHI::IRHIDevice& device)
    {
        device.WaitIdle();

        m_ShadowmapSets.clear();
        m_ShadowmapUniforms.clear();
        m_Pipeline.reset();
        m_FrameBuffer.reset();
        m_RenderPass.reset();
        m_ShadowmapPool.reset();
        m_ShadowmapLayout.reset();
    }

    void ShadowmapPass::UpdateData(const FD::FrameData& frame, uint32_t frameIndex,
                                    const HedgehogSettings::Settings& settings)
    {
        UpdateShadowmapMatrices(frame.m_Camera, settings, frame.m_ShadowLightDirection);

        const uint32_t cascades = settings.GetShadowmapSettings()->GetCascadesCount();

        for (size_t i = 0; i < cascades; ++i)
        {
            ShadowCascadeUniform ubo;
            ubo.m_ShadowMatrix = m_ShadowmapMatrices[i];
            m_ShadowmapUniforms[frameIndex][i]->CopyData(&ubo, sizeof(ubo));
        }
    }

    void ShadowmapPass::UpdateResources(RHI::IRHIDevice& device, const HedgehogSettings::Settings& settings,
                                         const ResourceManager& resourceManager)
    {
        if (!settings.GetShadowmapSettings()->IsDirty())
            return;

        UpdateFrameBuffer(device, resourceManager);
        UpdateViewports(settings);
    }

    void ShadowmapPass::UpdateFrameBuffer(RHI::IRHIDevice& device, const ResourceManager& resourceManager)
    {
        const auto& shadowMap = resourceManager.GetRHIShadowMap();

        m_FrameBuffer.reset();

        RHI::FramebufferDesc fbDesc;
        fbDesc.m_RenderPass      = m_RenderPass.get();
        fbDesc.m_DepthAttachment = &shadowMap;
        fbDesc.m_Width           = shadowMap.GetWidth();
        fbDesc.m_Height          = shadowMap.GetHeight();
        m_FrameBuffer = device.CreateFramebuffer(fbDesc);
    }

    void ShadowmapPass::UpdateViewports(const HedgehogSettings::Settings& settings)
    {
        const auto& shadowmapSettings = settings.GetShadowmapSettings();
        m_CascadesCount = shadowmapSettings->GetCascadesCount();
        m_ShadowmapSize = shadowmapSettings->GetShadowmapSize();
        const float sz  = static_cast<float>(m_ShadowmapSize);

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

    void ShadowmapPass::UpdateShadowmapMatrices(const FD::CameraData& camera,
                                                 const HedgehogSettings::Settings& settings,
                                                 const std::optional<HM::Vector3>& shadowLightDir)
    {
        const auto& shadowmapSettings  = settings.GetShadowmapSettings();
        const uint32_t cascadesCount  = shadowmapSettings->GetCascadesCount();
        const float cascadeSplitLambda = shadowmapSettings->GetCascadeSplitLambda();

        std::vector<float> cascadeSplits;

        const float nearClip  = camera.m_Near;
        const float farClip   = camera.m_Far;
        const float clipRange = farClip - nearClip;

        const float minZ  = nearClip;
        const float maxZ  = nearClip + clipRange;
        const float range = maxZ - minZ;
        const float ratio = maxZ / minZ;

        for (uint32_t i = 0; i < cascadesCount; ++i)
        {
            const float p       = (i + 1) / static_cast<float>(cascadesCount);
            const float log     = minZ * std::pow(ratio, p);
            const float uniform = minZ + range * p;
            const float d       = cascadeSplitLambda * (log - uniform) + uniform;
            cascadeSplits.push_back((d - nearClip) / clipRange);
        }

        float lastSplitDist = 0.0f;
        HM::Matrix4x4 camMatrix = camera.m_View * camera.m_Proj;
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
            if (shadowLightDir.has_value())
                lightDir = shadowLightDir.value();

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
