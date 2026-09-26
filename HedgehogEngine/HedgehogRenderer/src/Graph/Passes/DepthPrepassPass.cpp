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
                cmd.BindDescriptorSet(pipeline, 0, data.Context->Services->AllocateViewProjUniform(frame.Proj * frame.View));
                DrawOpaqueInstances(cmd, pipeline, frame);
                cmd.EndRendering();
            });
        }
    }

    PassTypeInfo GetDepthPrepassPassType()
    {
        return { { "depth" }, {}, &BuildDepthPrepass };
    }
}
