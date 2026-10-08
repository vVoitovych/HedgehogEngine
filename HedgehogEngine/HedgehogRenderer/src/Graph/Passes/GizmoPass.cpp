#include "GizmoPass.hpp"

#include "PassCommon.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHITexture.hpp"

namespace Renderer
{
    namespace
    {
        // Draws each overlay instance's world bounds as a wireframe box, then the frame's debug lines,
        // into color over what the view has drawn, tested against its depth but not writing it. A view
        // with neither records nothing, so the pass costs nothing until something is shown.
        void RecordGizmo(ForwardPassData& data, RHI::IRHICommandList& cmd)
        {
            if (!data.Context)
                return;
            const GraphFrameData& frame    = *data.Context->Frame;
            const bool            hasBoxes = !frame.OverlayInstances.empty();
            const bool            hasLines = frame.DebugLineVertices != nullptr && frame.DebugLineVertexCount >= 2;
            if (!hasBoxes && !hasLines)
                return;
            IGraphPassServices&      services = *data.Context->Services;
            RHI::IRHITexture&        color    = *data.Graph->GetTexture(data.Color);
            RHI::IRHITexture&        depth    = *data.Graph->GetTexture(data.Depth);

            RHI::RenderingAttachment colorAttachment;
            colorAttachment.Texture = &color;
            colorAttachment.LoadOp  = RHI::LoadOp::Load;
            colorAttachment.StoreOp = RHI::StoreOp::Store;

            RHI::RenderingAttachment depthAttachment;
            depthAttachment.Texture = &depth;
            depthAttachment.LoadOp  = RHI::LoadOp::Load;
            depthAttachment.StoreOp = RHI::StoreOp::Store;

            RHI::RenderingInfo info;
            info.ColorAttachments = { colorAttachment };
            info.DepthAttachment  = depthAttachment;
            info.Width            = color.GetWidth();
            info.Height           = color.GetHeight();
            cmd.BeginRendering(info);
            cmd.SetViewport({ 0.0f, 0.0f, static_cast<float>(color.GetWidth()),
                              static_cast<float>(color.GetHeight()), 0.0f, 1.0f });
            cmd.SetScissor({ 0, 0, color.GetWidth(), color.GetHeight() });
            const RHI::IRHIDescriptorSet& viewProj = services.AllocateViewProjUniform(frame.Proj * frame.View);

            if (hasBoxes)
            {
                const RHI::IRHIPipeline& pipeline = services.GetPipeline(EnginePipeline::Gizmo);
                cmd.BindPipeline(pipeline);
                cmd.BindVertexBuffers(0, { &services.GetGizmoBoxLines() }, { 0 });
                cmd.BindDescriptorSet(pipeline, 0, viewProj);
                for (const HX::RenderInstance& instance : frame.OverlayInstances)
                {
                    const HM::Matrix4x4 model = MakeGizmoBoxMatrix(instance.WorldBounds);
                    cmd.PushConstants(pipeline, RHI::ShaderStage::Vertex, 0, 16 * sizeof(float), model.GetBuffer());
                    cmd.Draw(GIZMO_BOX_LINE_VERTICES, 1, 0, 0);
                }
            }
            if (hasLines)
            {
                // One draw of every line, already in world space.
                const RHI::IRHIPipeline& pipeline = services.GetPipeline(EnginePipeline::DebugLines);
                cmd.BindPipeline(pipeline);
                cmd.BindVertexBuffers(0, { frame.DebugLineVertices }, { 0 });
                cmd.BindDescriptorSet(pipeline, 0, viewProj);
                cmd.Draw(frame.DebugLineVertexCount, 1, 0, 0);
            }
            cmd.EndRendering();
        }

        void BuildGizmo(RenderGraphRuntime& graph, PassInvocation& invocation)
        {
            graph.AddPass<ForwardPassData>(invocation.GetName(),
                [&](RGPassBuilder& pass, ForwardPassData& data)
                {
                    data.Depth = invocation.GetSlot("depth");
                    pass.DepthReadOnly(data.Depth);
                    data.Color   = pass.ColorTarget(invocation.GetSlot("color"));
                    data.Graph   = &graph;
                    data.Context = graph.GetFrameContext();
                    invocation.SetSlot("color", data.Color);
                },
                [](ForwardPassData& data, RHI::IRHICommandList& cmd) { RecordGizmo(data, cmd); });
        }
    }

    HM::Matrix4x4 MakeGizmoBoxMatrix(const HM::AABB& box)
    {
        const HM::Vector3 min    = box.GetMin();
        const HM::Vector3 extent = box.GetMax() - min;
        return HM::Matrix4x4::GetTranslation(min.x(), min.y(), min.z())
             * HM::Matrix4x4::GetScale(extent.x(), extent.y(), extent.z());
    }

    PassTypeInfo GetGizmoPassType()
    {
        return { { "color", "depth" }, {}, &BuildGizmo };
    }
}
