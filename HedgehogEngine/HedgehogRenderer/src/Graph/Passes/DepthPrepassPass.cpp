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
                const RHI::IRHIPipeline& pipeline =
                    data.Context->Services->GetPipeline(EnginePipeline::DepthPrepass);
                RHI::IRHITexture& depth = *data.Graph->GetTexture(data.Target);

                BeginDepthRendering(cmd, depth);
                cmd.BindPipeline(pipeline);
                cmd.SetViewport({ 0.0f, 0.0f, static_cast<float>(depth.GetWidth()),
                                  static_cast<float>(depth.GetHeight()), 0.0f, 1.0f });
                cmd.SetScissor({ 0, 0, depth.GetWidth(), depth.GetHeight() });
                const RHI::IRHIDescriptorSet& viewProj =
                    data.Context->Services->AllocateViewProjUniform(frame.Proj * frame.View);
                cmd.BindDescriptorSet(pipeline, 0, viewProj);
                DrawOpaqueInstances(cmd, pipeline, frame);

                // Skinned instances after the rigid ones. The skinned layout's push constants differ,
                // so set 0 is bound again.
                if (CanDrawSkinned(frame))
                {
                    const RHI::IRHIPipeline& skinned =
                        data.Context->Services->GetPipeline(EnginePipeline::DepthPrepassSkinned);
                    cmd.BindPipeline(skinned);
                    cmd.BindDescriptorSet(skinned, 0, viewProj);
                    cmd.BindDescriptorSet(skinned, 1, *frame.JointPalette);
                    DrawSkinnedInstances(cmd, skinned, frame);
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
