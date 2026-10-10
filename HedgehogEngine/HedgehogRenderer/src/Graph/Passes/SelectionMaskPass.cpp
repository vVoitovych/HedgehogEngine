#include "SelectionMaskPass.hpp"

#include "PassCommon.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHITexture.hpp"

namespace Renderer
{
    namespace
    {
        // Draws the overlay instances of one kind (rigid or skinned), with the model matrix (and the
        // palette offset for a skinned one) as the push constants; the caller binds the pipeline,
        // its sets and the vertex streams.
        void DrawOverlay(RHI::IRHICommandList& cmd, const RHI::IRHIPipeline& pipeline, const GraphFrameData& frame,
                         bool skinned)
        {
            for (const HX::RenderInstance& instance : frame.OverlayInstances)
            {
                if ((instance.JointCount > 0) != skinned || instance.MeshIndex >= frame.Meshes.size())
                    continue;
                const MeshDrawRange& mesh = frame.Meshes[instance.MeshIndex];
                if (skinned)
                {
                    const SkinnedPushConstants constants = MakeSkinnedPushConstants(instance);
                    cmd.PushConstants(pipeline, RHI::ShaderStage::Vertex, 0, sizeof(constants), &constants);
                }
                else
                {
                    cmd.PushConstants(pipeline, RHI::ShaderStage::Vertex, 0, 16 * sizeof(float),
                                      instance.WorldMatrix.GetBuffer());
                }
                cmd.DrawIndexed(mesh.IndexCount, 1, mesh.FirstIndex, static_cast<int32_t>(mesh.VertexOffset), 0);
            }
        }

        void RecordSelectionMask(TargetPassData& data, RHI::IRHICommandList& cmd)
        {
            if (!data.Context)
                return;
            const GraphFrameData& frame = *data.Context->Frame;
            if (frame.OverlayInstances.empty() || !frame.Positions || !frame.Indices)
                return;
            bool rigid   = false;
            bool skinned = false;
            for (const HX::RenderInstance& instance : frame.OverlayInstances)
                (instance.JointCount > 0 ? skinned : rigid) = true;
            skinned = skinned && frame.JointPalette && frame.Joints && frame.Weights;

            IGraphPassServices& services = *data.Context->Services;
            RHI::IRHITexture&   mask     = *data.Graph->GetTexture(data.Target);

            RHI::RenderingAttachment attachment;
            attachment.Texture     = &mask;
            attachment.LoadOp      = RHI::LoadOp::Clear;
            attachment.StoreOp     = RHI::StoreOp::Store;
            attachment.Clear.Color = { 0.0f, 0.0f, 0.0f, 0.0f };
            RHI::RenderingInfo info;
            info.ColorAttachments = { attachment };
            info.Width            = mask.GetWidth();
            info.Height           = mask.GetHeight();
            cmd.BeginRendering(info);
            cmd.SetViewport({ 0.0f, 0.0f, static_cast<float>(mask.GetWidth()), static_cast<float>(mask.GetHeight()),
                              0.0f, 1.0f });
            cmd.SetScissor({ 0, 0, mask.GetWidth(), mask.GetHeight() });
            const RHI::IRHIDescriptorSet& viewProj = services.AllocateViewProjUniform(frame.Proj * frame.View);

            if (rigid)
            {
                const RHI::IRHIPipeline& pipeline = services.GetPipeline(EnginePipeline::SelectionMask);
                cmd.BindPipeline(pipeline);
                cmd.BindDescriptorSet(pipeline, 0, viewProj);
                cmd.BindVertexBuffers(0, { frame.Positions }, { 0 });
                cmd.BindIndexBuffer(*frame.Indices, RHI::IndexType::Uint32);
                DrawOverlay(cmd, pipeline, frame, false);
            }
            if (skinned)
            {
                const RHI::IRHIPipeline& pipeline = services.GetPipeline(EnginePipeline::SelectionMaskSkinned);
                cmd.BindPipeline(pipeline);
                cmd.BindDescriptorSet(pipeline, 0, viewProj);
                cmd.BindDescriptorSet(pipeline, 1, *frame.JointPalette);
                cmd.BindVertexBuffers(0, { frame.Positions, frame.Joints, frame.Weights }, { 0, 0, 0 });
                cmd.BindIndexBuffer(*frame.Indices, RHI::IndexType::Uint32);
                DrawOverlay(cmd, pipeline, frame, true);
            }
            cmd.EndRendering();
        }

        void BuildSelectionMask(RenderGraphRuntime& graph, PassInvocation& invocation)
        {
            graph.AddPass<TargetPassData>(invocation.GetName(),
                [&](RGPassBuilder& pass, TargetPassData& data)
                {
                    data.Target  = pass.ColorTarget(invocation.GetSlot("mask"));
                    data.Graph   = &graph;
                    data.Context = graph.GetFrameContext();
                    invocation.SetSlot("mask", data.Target);
                },
                [](TargetPassData& data, RHI::IRHICommandList& cmd) { RecordSelectionMask(data, cmd); });
        }
    }

    PassTypeInfo GetSelectionMaskPassType()
    {
        return { { "mask" }, {}, &BuildSelectionMask };
    }
}
