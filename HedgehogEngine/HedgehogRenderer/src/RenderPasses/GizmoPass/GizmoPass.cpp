#include "GizmoPass.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "ResourceManager/ResourceManager.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"

#include "Pipeline/ShaderLoader.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHIRenderPass.hpp"
#include "RHI/api/IRHIFramebuffer.hpp"
#include "RHI/api/IRHIPipeline.hpp"
#include "RHI/api/IRHIBuffer.hpp"
#include "RHI/api/IRHITexture.hpp"

#include <algorithm>

namespace Renderer
{
namespace
{
    constexpr uint32_t kMaxGizmoVertices = 8192;

    HM::Vector3 TransformPoint(const HM::Matrix4x4& m, const HM::Vector3& p)
    {
        // Row-vector convention (matches the mesh/transform pipeline): worldPoint = localPoint * M.
        const HM::Vector4 r = HM::Vector4(p.x(), p.y(), p.z(), 1.0f) * m;
        return HM::Vector3(r.x(), r.y(), r.z());
    }
}

    GizmoPass::GizmoPass(RHI::IRHIDevice& device, const ResourceManager& resourceManager,
                          const FS::FileSystemManager& fileSystem)
    {
        const auto sd = ShaderLoader::Load(device,
            "engine://HedgehogEngine/HedgehogRenderer/assets/Shaders/Gizmo.shader",
            fileSystem);

        // Color-only render pass that loads (preserves) the forward result and draws lines over it.
        RHI::RenderPassDesc rpDesc;
        rpDesc.ColorAttachments.push_back(RHI::AttachmentDesc{
            resourceManager.GetSceneColorBuffer().GetFormat(),
            RHI::LoadOp::Load,
            RHI::StoreOp::Store,
            RHI::LoadOp::DontCare,
            RHI::StoreOp::DontCare,
            RHI::ImageLayout::ColorAttachment,
            RHI::ImageLayout::ColorAttachment
        });
        m_RenderPass = device.CreateRenderPass(rpDesc);

        auto pipelineDesc       = sd.Pipeline;   // no descriptor sets: view-proj is a push constant
        pipelineDesc.RenderPass = m_RenderPass.get();
        m_Pipeline = device.CreateGraphicsPipeline(pipelineDesc);

        for (auto& buffer : m_VertexBuffers)
        {
            buffer = device.CreateBuffer(
                static_cast<size_t>(kMaxGizmoVertices) * sizeof(GizmoVertex),
                RHI::BufferUsage::VertexBuffer,
                RHI::MemoryUsage::CpuToGpu);
        }

        CreateFramebuffer(device, resourceManager);
    }

    GizmoPass::~GizmoPass()
    {
    }

    void GizmoPass::CreateFramebuffer(RHI::IRHIDevice& device, const ResourceManager& resourceManager)
    {
        const auto& colorBuffer = resourceManager.GetSceneColorBuffer();

        RHI::FramebufferDesc fbDesc;
        fbDesc.RenderPass       = m_RenderPass.get();
        fbDesc.ColorAttachments = { &colorBuffer };
        fbDesc.Width            = colorBuffer.GetWidth();
        fbDesc.Height           = colorBuffer.GetHeight();
        m_FrameBuffer = device.CreateFramebuffer(fbDesc);
    }

    void GizmoPass::BuildLines(const std::vector<HedgehogEngine::LightData>& lights,
                                const std::optional<HM::Matrix4x4>&           selected)
    {
        m_Lines.clear();

        const auto addLine = [this](const HM::Vector3& a, const HM::Vector3& b, const HM::Vector3& c)
        {
            m_Lines.push_back({ a.x(), a.y(), a.z(), c.x(), c.y(), c.z() });
            m_Lines.push_back({ b.x(), b.y(), b.z(), c.x(), c.y(), c.z() });
        };

        // Selected entity: RGB transform axes + oriented box (approximate bounds — a unit box in
        // local space; a true per-mesh AABB arrives with picking in a later phase).
        if (selected)
        {
            const HM::Matrix4x4& m = *selected;
            const HM::Vector3 origin(m[3][0], m[3][1], m[3][2]);
            const HM::Vector3 xAxis = HM::Vector3(m[0][0], m[0][1], m[0][2]).Normalize();
            const HM::Vector3 yAxis = HM::Vector3(m[1][0], m[1][1], m[1][2]).Normalize();
            const HM::Vector3 zAxis = HM::Vector3(m[2][0], m[2][1], m[2][2]).Normalize();

            constexpr float axisLen = 1.5f;
            addLine(origin, origin + xAxis * axisLen, HM::Vector3(1.0f, 0.15f, 0.15f));
            addLine(origin, origin + yAxis * axisLen, HM::Vector3(0.15f, 1.0f, 0.15f));
            addLine(origin, origin + zAxis * axisLen, HM::Vector3(0.2f, 0.4f, 1.0f));

            HM::Vector3 c[8];
            for (int i = 0; i < 8; ++i)
            {
                const float x = (i & 4) ? 0.5f : -0.5f;
                const float y = (i & 2) ? 0.5f : -0.5f;
                const float z = (i & 1) ? 0.5f : -0.5f;
                c[i] = TransformPoint(m, HM::Vector3(x, y, z));
            }
            static const int edges[12][2] = {
                {0,1},{0,2},{0,4},{1,3},{1,5},{2,3},{2,6},{3,7},{4,5},{4,6},{5,7},{6,7}
            };
            const HM::Vector3 boxColor(1.0f, 0.6f, 0.1f);
            for (const auto& e : edges)
                addLine(c[e[0]], c[e[1]], boxColor);
        }

        // Light glyphs: a small 3-axis cross at each light, plus a ray for directional lights.
        for (const auto& light : lights)
        {
            const HM::Vector3 p = light.Position;
            const HM::Vector3 col = light.Color;
            constexpr float s = 0.25f;
            addLine(p - HM::Vector3(s, 0.0f, 0.0f), p + HM::Vector3(s, 0.0f, 0.0f), col);
            addLine(p - HM::Vector3(0.0f, s, 0.0f), p + HM::Vector3(0.0f, s, 0.0f), col);
            addLine(p - HM::Vector3(0.0f, 0.0f, s), p + HM::Vector3(0.0f, 0.0f, s), col);

            if (light.Type == 0) // directional
            {
                const HM::Vector3 d = light.Direction;
                const float lenSq = d.x() * d.x() + d.y() * d.y() + d.z() * d.z();
                if (lenSq > 1e-6f)
                    addLine(p, p + d.Normalize() * 1.5f, HM::Vector3(1.0f, 1.0f, 0.4f));
            }
        }

        if (m_Lines.size() > kMaxGizmoVertices)
            m_Lines.resize(kMaxGizmoVertices);
    }

    void GizmoPass::Render(const HedgehogEngine::CameraData&              camera,
                            const std::vector<HedgehogEngine::LightData>&  lights,
                            const std::optional<HM::Matrix4x4>&            selected,
                            RHI::IRHICommandList& cmd, uint32_t frameIndex)
    {
        BuildLines(lights, selected);
        if (m_Lines.empty())
            return;

        const uint32_t vertexCount = static_cast<uint32_t>(m_Lines.size());
        m_VertexBuffers[frameIndex]->CopyData(m_Lines.data(), vertexCount * sizeof(GizmoVertex));

        RHI::ClearValue colorClear;
        colorClear.Color = { 0.0f, 0.0f, 0.0f, 1.0f }; // ignored (LoadOp::Load), required by the API

        cmd.BeginRenderPass(*m_RenderPass, *m_FrameBuffer, { colorClear });

        const uint32_t width  = m_FrameBuffer->GetWidth();
        const uint32_t height = m_FrameBuffer->GetHeight();
        cmd.BindPipeline(*m_Pipeline);
        cmd.SetViewport({ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f });
        cmd.SetScissor({ 0, 0, width, height });

        const HM::Matrix4x4 viewProj = camera.Proj * camera.View;
        cmd.PushConstants(*m_Pipeline, RHI::ShaderStage::Vertex, 0,
                          static_cast<uint32_t>(sizeof(HM::Matrix4x4)), &viewProj);

        auto* vb = m_VertexBuffers[frameIndex].get();
        cmd.BindVertexBuffers(0, { vb }, { 0 });
        cmd.Draw(vertexCount, 1, 0, 0);

        cmd.EndRenderPass();
    }

    void GizmoPass::Cleanup(RHI::IRHIDevice& device)
    {
        device.WaitIdle();

        for (auto& buffer : m_VertexBuffers)
            buffer.reset();
        m_FrameBuffer.reset();
        m_Pipeline.reset();
        m_RenderPass.reset();
    }

    void GizmoPass::ResizeResources(RHI::IRHIDevice& device, const ResourceManager& resourceManager)
    {
        m_FrameBuffer.reset();
        CreateFramebuffer(device, resourceManager);
    }
}
