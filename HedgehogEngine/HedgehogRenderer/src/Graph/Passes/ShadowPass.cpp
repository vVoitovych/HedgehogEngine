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
        using CascadeSets = std::array<const RHI::IRHIDescriptorSet*, MAX_SHADOW_CASCADES>;

        // Records draw into every cascade's tile: its viewport and its viewProj at set 0, through
        // pipeline (the twin bound last; their layouts are compatible).
        template<typename DrawFn>
        void ForEachCascade(RHI::IRHICommandList& cmd, const ShadowCascades& cascades, uint32_t size,
                            const RHI::IRHIPipeline& pipeline, const CascadeSets& viewProj, DrawFn&& draw)
        {
            for (uint32_t i = 0; i < cascades.Count; ++i)
            {
                const ShadowCascadeViewport& tile = cascades.Viewports[i];
                cmd.SetViewport({ tile.X, tile.Y, tile.Width, tile.Height, 0.0f, 1.0f });
                cmd.SetScissor({ 0, 0, size, size });
                cmd.BindDescriptorSet(pipeline, 0, *viewProj[i]);
                draw();
            }
        }

        void BuildShadow(RenderGraphRuntime& graph, PassInvocation& invocation)
        {
            AddDepthOnlyPass(graph, invocation, "shadowMap", [](TargetPassData& data, RHI::IRHICommandList& cmd)
            {
                if (!data.Context)
                    return;
                const GraphFrameData& frame     = *data.Context->Frame;
                IGraphPassServices&   services  = *data.Context->Services;
                RHI::IRHITexture&     shadowMap = *data.Graph->GetTexture(data.Target);

                const uint32_t       size     = BeginDepthRendering(cmd, shadowMap);
                // The shared phase's cascades, which the forward passes read the atlas with.
                const ShadowCascades cascades = frame.Cascades ? *frame.Cascades : ComputeShadowCascades(frame, size);
                CascadeSets          viewProj{};
                for (uint32_t i = 0; i < cascades.Count; ++i)
                    viewProj[i] = &services.AllocateViewProjUniform(cascades.ViewProj[i]);

                // Rigid casters, each through the twin its material's sidedness asks for.
                SidedPipeline            rigid(services.GetPipeline(EnginePipeline::Shadow),
                                               services.GetPipeline(EnginePipeline::ShadowDoubleSided));
                const RHI::IRHIPipeline& pipeline = rigid.Bind(cmd);
                ForEachCascade(cmd, cascades, size, pipeline, viewProj,
                               [&] { DrawRigidInstances(cmd, rigid, frame, frame.OpaqueInstances); });

                // Skinned casters after every cascade's rigid ones, so the skinned pipeline is bound
                // once. Its push constants differ, so each cascade's set 0 is bound again.
                if (CanDrawSkinned(frame))
                {
                    SidedPipeline            skinned(services.GetPipeline(EnginePipeline::ShadowSkinned),
                                                     services.GetPipeline(EnginePipeline::ShadowSkinnedDoubleSided));
                    const RHI::IRHIPipeline& bound = skinned.Bind(cmd);
                    cmd.BindDescriptorSet(bound, 1, *frame.JointPalette);
                    ForEachCascade(cmd, cascades, size, bound, viewProj,
                                   [&] { DrawSkinnedInstances(cmd, skinned, frame, frame.SkinnedInstances); });
                }

                // Alpha-tested casters last: holes where their material's alpha is below its cutoff.
                if (!frame.CutoffInstances.empty() && frame.Positions && frame.TexCoords && frame.Indices)
                {
                    SidedPipeline            cutoff(services.GetPipeline(EnginePipeline::ShadowCutoff),
                                                    services.GetPipeline(EnginePipeline::ShadowCutoffDoubleSided));
                    const RHI::IRHIPipeline& bound = cutoff.Bind(cmd);
                    ForEachCascade(cmd, cascades, size, bound, viewProj,
                                   [&] { DrawCutoffInstances(cmd, cutoff, frame, frame.CutoffInstances, false, 1); });
                }
                if (CanDrawSkinned(frame, frame.SkinnedCutoffInstances) && frame.TexCoords)
                {
                    SidedPipeline cutoff(services.GetPipeline(EnginePipeline::ShadowCutoffSkinned),
                                         services.GetPipeline(EnginePipeline::ShadowCutoffSkinnedDoubleSided));
                    const RHI::IRHIPipeline& bound = cutoff.Bind(cmd);
                    cmd.BindDescriptorSet(bound, 1, *frame.JointPalette);
                    ForEachCascade(cmd, cascades, size, bound, viewProj,
                                   [&] { DrawCutoffInstances(cmd, cutoff, frame, frame.SkinnedCutoffInstances, true, 2); });
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
