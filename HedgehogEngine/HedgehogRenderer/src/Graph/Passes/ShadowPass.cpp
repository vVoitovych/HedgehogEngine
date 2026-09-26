#include "ShadowPass.hpp"

#include "PassCommon.hpp"

#include "HedgehogRenderer/Graph/ShadowCascades.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHITexture.hpp"

namespace Renderer
{
    namespace
    {
        void BuildShadow(RenderGraphRuntime& graph, PassInvocation& invocation)
        {
            AddDepthOnlyPass(graph, invocation, "shadowMap", [](TargetPassData& data, RHI::IRHICommandList& cmd)
            {
                if (!data.Context)
                    return;
                const GraphFrameData& frame    = *data.Context->Frame;
                const RHI::IRHIPipeline& pipeline =
                    data.Context->Services->GetPipeline(EnginePipeline::Shadow);
                RHI::IRHITexture& shadowMap = *data.Graph->GetTexture(data.Target);

                const uint32_t       size     = BeginDepthRendering(cmd, shadowMap);
                const ShadowCascades cascades = ComputeShadowCascades(frame, size);
                cmd.BindPipeline(pipeline);
                for (uint32_t i = 0; i < cascades.Count; ++i)
                {
                    const ShadowCascadeViewport& tile = cascades.Viewports[i];
                    cmd.SetViewport({ tile.X, tile.Y, tile.Width, tile.Height, 0.0f, 1.0f });
                    cmd.SetScissor({ 0, 0, size, size });
                    cmd.BindDescriptorSet(pipeline, 0, data.Context->Services->AllocateViewProjUniform(cascades.ViewProj[i]));
                    DrawOpaqueInstances(cmd, pipeline, frame);
                }
                cmd.EndRendering();
            });
        }
    }

    PassTypeInfo GetShadowPassType()
    {
        return { { "shadowMap" }, {}, &BuildShadow };
    }
}
