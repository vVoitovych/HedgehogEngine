#pragma once

#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"

#include "TestRHIDoubles.hpp"

#include "RHI/api/IRHIDescriptor.hpp"
#include "RHI/api/IRHIPipeline.hpp"

#include <vector>

// Stand-ins for what the renderer hands the engine passes through a GraphFrameContext, shared by
// the pass-recording and shared-phase tests.
namespace RGTest
{
    class FakePipeline final : public RHI::IRHIPipeline
    {
    };

    class FakeDescriptorSet final : public RHI::IRHIDescriptorSet
    {
    public:
        void WriteUniformBuffer(uint32_t, const RHI::IRHIBuffer&, size_t, size_t) override {}
        void WriteStorageBuffer(uint32_t, const RHI::IRHIBuffer&, size_t, size_t) override {}
        void WriteTexture(uint32_t, const RHI::IRHITexture&, const RHI::IRHISampler&) override {}
        void Flush() override {}
    };

    // Stands in for GraphPassServices: fixed pipelines, and a record of every uniform uploaded.
    class FakeServices final : public Renderer::IGraphPassServices
    {
    public:
        const RHI::IRHIPipeline& GetPipeline(Renderer::EnginePipeline pipeline) const override
        {
            switch (pipeline)
            {
                case Renderer::EnginePipeline::DepthPrepass: return m_Depth;
                case Renderer::EnginePipeline::Shadow:       return m_Shadow;
                case Renderer::EnginePipeline::Gizmo:        return m_Gizmo;
                default:                                     return m_Forward;
            }
        }
        RHI::IRHIBuffer& GetGizmoBoxLines() override { return m_GizmoBoxLines; }
        const RHI::IRHIDescriptorSet& AllocateViewProjUniform(const HM::Matrix4x4& viewProj) override
        {
            UploadedFirstElements.push_back(viewProj.GetBuffer()[0]);
            return m_Set;
        }
        const RHI::IRHIDescriptorSet& AllocateForwardViewUniform(const Renderer::ForwardViewUniform&) override
        {
            ++ForwardViewUploads;
            return m_Set;
        }
        const RHI::IRHIDescriptorSet& AllocateSceneLightsUniform(const Renderer::SceneLightsUniform& uniform) override
        {
            SceneLightCounts.push_back(uniform.LightCount);
            return m_SceneLights;
        }

        std::vector<float>   UploadedFirstElements;
        int                  ForwardViewUploads = 0;
        std::vector<int32_t> SceneLightCounts;

    private:
        FakePipeline      m_Depth;
        FakePipeline      m_Shadow;
        FakePipeline      m_Forward;
        FakePipeline      m_Gizmo;
        TestBuffer        m_GizmoBoxLines{ Renderer::GIZMO_BOX_LINE_VERTICES * 12 };
        FakeDescriptorSet m_Set;
        FakeDescriptorSet m_SceneLights;
    };

    inline HX::RenderInstance Instance(uint64_t meshIndex, uint32_t layer = 0)
    {
        HX::RenderInstance instance;
        instance.MeshIndex = meshIndex;
        instance.Layer     = layer;
        return instance;
    }

    inline Renderer::GraphFrameData MakeFrame(uint32_t cascades)
    {
        Renderer::GraphFrameData frame;
        frame.View = HM::Matrix4x4::LookAt(HM::Vector3(0.0f, -10.0f, 2.0f), HM::Vector3(0.0f, 0.0f, 0.0f),
                                           HM::Vector3(0.0f, 0.0f, 1.0f));
        frame.Proj                     = HM::Matrix4x4::Perspective(60.0f, 16.0f / 9.0f, 0.1f, 100.0f);
        frame.NearPlane                = 0.1f;
        frame.FarPlane                 = 100.0f;
        frame.ShadowCascadeCount       = cascades;
        frame.ShadowCascadeSplitLambda = 0.75f;
        frame.ShadowLightDirection     = HM::Vector3(0.3f, 0.2f, -1.0f);
        return frame;
    }
}
