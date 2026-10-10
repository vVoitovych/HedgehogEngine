#include "DepthPrepassPass.hpp"

#include "PassCommon.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHITexture.hpp"

namespace Renderer
{
    namespace
    {
        void BuildDepthPrepass(RenderGraphRuntime& graph, PassInvocation& invocation)
        {
            AddDepthOnlyPass(graph, invocation, "depth", [](TargetPassData& data, RHI::IRHICommandList& cmd)
            {
                if (!data.Context)
                    return;
                const GraphFrameData& frame    = *data.Context->Frame;
                IGraphPassServices&   services = *data.Context->Services;
                RHI::IRHITexture&     depth    = *data.Graph->GetTexture(data.Target);

                // Opaque instances, each through the pipeline its material's sidedness asks for.
                BeginDepthRendering(cmd, depth);
                SidedPipeline            opaque(services.GetPipeline(EnginePipeline::DepthPrepass),
                                                services.GetPipeline(EnginePipeline::DepthPrepassDoubleSided));
                const RHI::IRHIPipeline& pipeline = opaque.Bind(cmd);
                cmd.SetViewport({ 0.0f, 0.0f, static_cast<float>(depth.GetWidth()),
                                  static_cast<float>(depth.GetHeight()), 0.0f, 1.0f });
                cmd.SetScissor({ 0, 0, depth.GetWidth(), depth.GetHeight() });
                const RHI::IRHIDescriptorSet& viewProj = services.AllocateViewProjUniform(frame.Proj * frame.View);
                cmd.BindDescriptorSet(pipeline, 0, viewProj);
                DrawRigidInstances(cmd, opaque, frame, frame.OpaqueInstances);

                // Skinned instances after the rigid ones. The skinned layout's push constants differ,
                // so set 0 is bound again.
                if (CanDrawSkinned(frame))
                {
                    SidedPipeline            skinned(services.GetPipeline(EnginePipeline::DepthPrepassSkinned),
                                                     services.GetPipeline(EnginePipeline::DepthPrepassSkinnedDoubleSided));
                    const RHI::IRHIPipeline& bound = skinned.Bind(cmd);
                    cmd.BindDescriptorSet(bound, 0, viewProj);
                    cmd.BindDescriptorSet(bound, 1, *frame.JointPalette);
                    DrawSkinnedInstances(cmd, skinned, frame, frame.SkinnedInstances);
                }

                // Alpha-tested instances last, through the cutoff pipelines and their materials.
                if (!frame.CutoffInstances.empty() && frame.Positions && frame.TexCoords && frame.Indices)
                {
                    SidedPipeline            cutoff(services.GetPipeline(EnginePipeline::DepthPrepassCutoff),
                                                    services.GetPipeline(EnginePipeline::DepthPrepassCutoffDoubleSided));
                    const RHI::IRHIPipeline& bound = cutoff.Bind(cmd);
                    cmd.BindDescriptorSet(bound, 0, viewProj);
                    DrawCutoffInstances(cmd, cutoff, frame, frame.CutoffInstances, false, 1);
                }
                if (CanDrawSkinned(frame, frame.SkinnedCutoffInstances) && frame.TexCoords)
                {
                    SidedPipeline cutoff(services.GetPipeline(EnginePipeline::DepthPrepassCutoffSkinned),
                                         services.GetPipeline(EnginePipeline::DepthPrepassCutoffSkinnedDoubleSided));
                    const RHI::IRHIPipeline& bound = cutoff.Bind(cmd);
                    cmd.BindDescriptorSet(bound, 0, viewProj);
                    cmd.BindDescriptorSet(bound, 1, *frame.JointPalette);
                    DrawCutoffInstances(cmd, cutoff, frame, frame.SkinnedCutoffInstances, true, 2);
                }
                cmd.EndRendering();
            });
        }
    }

    PassTypeInfo GetDepthPrepassPassType()
    {
        return { { "depth" }, {}, &BuildDepthPrepass };
    }
}
