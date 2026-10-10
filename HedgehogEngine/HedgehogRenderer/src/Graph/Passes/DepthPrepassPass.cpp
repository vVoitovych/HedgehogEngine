#include "DepthPrepassPass.hpp"

#include "PassCommon.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHITexture.hpp"

namespace Renderer
{
    namespace
    {
        // Draws Cutoff instances with the cutoff pipelines, which sample the base colour through the
        // material set (at materialSet) and discard below the material's cutoff. Rigid ones read the
        // positions and UVs; skinned ones the positions, joints, weights and UVs and push their
        // palette offset. The material set is bound again only when it changes; an instance whose
        // mesh or material has nothing to draw with is skipped.
        void DrawCutoffInstances(RHI::IRHICommandList& cmd, SidedPipeline& pipeline, const GraphFrameData& frame,
                                 std::span<const HX::RenderInstance> instances, bool skinned, uint32_t materialSet)
        {
            if (skinned)
                cmd.BindVertexBuffers(0, { frame.Positions, frame.Joints, frame.Weights, frame.TexCoords }, { 0, 0, 0, 0 });
            else
                cmd.BindVertexBuffers(0, { frame.Positions, frame.TexCoords }, { 0, 0 });
            cmd.BindIndexBuffer(*frame.Indices, RHI::IndexType::Uint32);

            uint64_t boundMaterial = UINT64_MAX;
            for (const HX::RenderInstance& instance : instances)
            {
                if (instance.MeshIndex >= frame.Meshes.size() || instance.MaterialIndex >= frame.MaterialSets.size()
                    || !frame.MaterialSets[instance.MaterialIndex])
                {
                    continue;
                }
                const RHI::IRHIPipeline& bound = pipeline.Use(cmd, frame, instance);
                if (instance.MaterialIndex != boundMaterial)
                {
                    cmd.BindDescriptorSet(bound, materialSet, *frame.MaterialSets[instance.MaterialIndex]);
                    boundMaterial = instance.MaterialIndex;
                }
                const MeshDrawRange& mesh = frame.Meshes[instance.MeshIndex];
                if (skinned)
                {
                    const SkinnedPushConstants constants = MakeSkinnedPushConstants(instance);
                    cmd.PushConstants(bound, RHI::ShaderStage::Vertex, 0, sizeof(constants), &constants);
                }
                else
                {
                    cmd.PushConstants(bound, RHI::ShaderStage::Vertex, 0, 16 * sizeof(float), instance.WorldMatrix.GetBuffer());
                }
                cmd.DrawIndexed(mesh.IndexCount, 1, mesh.FirstIndex, static_cast<int32_t>(mesh.VertexOffset), 0);
            }
        }

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
