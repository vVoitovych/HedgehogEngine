#include "ShadowPass.hpp"

#include "PassCommon.hpp"

#include "HedgehogRenderer/Graph/ShadowCascades.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHITexture.hpp"

#include <array>

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
                // The shared phase's cascades, which the forward passes read the atlas with.
                const ShadowCascades cascades = frame.Cascades ? *frame.Cascades : ComputeShadowCascades(frame, size);
                std::array<const RHI::IRHIDescriptorSet*, MAX_SHADOW_CASCADES> viewProj{};
                cmd.BindPipeline(pipeline);
                for (uint32_t i = 0; i < cascades.Count; ++i)
                {
                    const ShadowCascadeViewport& tile = cascades.Viewports[i];
                    cmd.SetViewport({ tile.X, tile.Y, tile.Width, tile.Height, 0.0f, 1.0f });
                    cmd.SetScissor({ 0, 0, size, size });
                    viewProj[i] = &data.Context->Services->AllocateViewProjUniform(cascades.ViewProj[i]);
                    cmd.BindDescriptorSet(pipeline, 0, *viewProj[i]);
                    DrawOpaqueInstances(cmd, pipeline, frame);
                }

                // Skinned casters after every cascade's rigid ones, so the skinned pipeline is bound
                // once. Its push constants differ, so each cascade's set 0 is bound again.
                if (CanDrawSkinned(frame))
                {
                    const RHI::IRHIPipeline& skinned =
                        data.Context->Services->GetPipeline(EnginePipeline::ShadowSkinned);
                    cmd.BindPipeline(skinned);
                    cmd.BindDescriptorSet(skinned, 1, *frame.JointPalette);
                    for (uint32_t i = 0; i < cascades.Count; ++i)
                    {
                        const ShadowCascadeViewport& tile = cascades.Viewports[i];
                        cmd.SetViewport({ tile.X, tile.Y, tile.Width, tile.Height, 0.0f, 1.0f });
                        cmd.SetScissor({ 0, 0, size, size });
                        cmd.BindDescriptorSet(skinned, 0, *viewProj[i]);
                        DrawSkinnedInstances(cmd, skinned, frame);
                    }
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
