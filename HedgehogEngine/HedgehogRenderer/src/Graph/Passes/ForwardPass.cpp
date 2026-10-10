#include "ForwardPass.hpp"

#include "PassCommon.hpp"

#include "HedgehogRenderer/Graph/GraphAssetVocabulary.hpp"
#include "HedgehogRenderer/Graph/ShadowCascades.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHITexture.hpp"

#include <cassert>
#include <cstdint>
#include <optional>
#include <span>

namespace Renderer
{
    namespace
    {
        // Colour cleared to opaque black (or loaded, to blend over), drawn against the prepass depth
        // without writing it.
        void BeginForwardRendering(RHI::IRHICommandList& cmd, RHI::IRHITexture& color, RHI::IRHITexture& depth,
                                   bool clear = true)
        {
            RHI::RenderingAttachment colorAttachment;
            colorAttachment.Texture     = &color;
            colorAttachment.LoadOp      = clear ? RHI::LoadOp::Clear : RHI::LoadOp::Load;
            colorAttachment.StoreOp     = RHI::StoreOp::Store;
            colorAttachment.Clear.Color = { 0.0f, 0.0f, 0.0f, 1.0f };

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
        }

        // Draws instances with their materials, rebinding set 1 only when the material changes
        // between consecutive instances, each through the twin its material's sidedness asks for.
        // Skinned instances push their palette offset as well. A Cutoff material's fragments below
        // its cutoff are discarded by the shader.
        void DrawLitInstances(RHI::IRHICommandList& cmd, SidedPipeline& pipeline, const GraphFrameData& frame,
                              std::span<const HX::RenderInstance> instances, bool skinned)
        {
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
                    cmd.BindDescriptorSet(bound, 1, *frame.MaterialSets[instance.MaterialIndex]);
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
                    cmd.PushConstants(bound, RHI::ShaderStage::Vertex, 0, 16 * sizeof(float),
                                      instance.WorldMatrix.GetBuffer());
                }
                cmd.DrawIndexed(mesh.IndexCount, 1, mesh.FirstIndex, static_cast<int32_t>(mesh.VertexOffset), 0);
            }
        }

        // Set 3: the sun's shadow (the frame's uniform, unshadowed without a shadow view) and the
        // atlas, then the environment's image-based lighting.
        const RHI::IRHIDescriptorSet& AllocateLighting(const ForwardPassData& data, IGraphPassServices& services,
                                                       const GraphFrameData& frame)
        {
            ShadowUniform        unshadowed;
            const ShadowUniform* shadow = frame.Shadow;
            if (!shadow)
            {
                unshadowed = MakeUnshadowedUniform();
                shadow     = &unshadowed;
            }
            return services.AllocateForwardLighting(*shadow, *data.Graph->GetTexture(data.ShadowMap), frame.Environment);
        }

        void RecordForward(ForwardPassData& data, RHI::IRHICommandList& cmd)
        {
            if (!data.Context)
                return;
            const GraphFrameData&    frame    = *data.Context->Frame;
            IGraphPassServices&      services = *data.Context->Services;
            // With cullBackFaces: false every instance draws double-sided, else as its material asks.
            const RHI::IRHIPipeline& doubleSided = services.GetPipeline(EnginePipeline::ForwardDoubleSided);
            SidedPipeline            lit(data.CullBackFaces ? services.GetPipeline(EnginePipeline::Forward) : doubleSided,
                                         doubleSided);
            RHI::IRHITexture& color = *data.Graph->GetTexture(data.Color);
            RHI::IRHITexture& depth = *data.Graph->GetTexture(data.Depth);

            BeginForwardRendering(cmd, color, depth);
            const RHI::IRHIPipeline& pipeline = lit.Bind(cmd);
            cmd.SetViewport({ 0.0f, 0.0f, static_cast<float>(color.GetWidth()),
                              static_cast<float>(color.GetHeight()), 0.0f, 1.0f });
            cmd.SetScissor({ 0, 0, color.GetWidth(), color.GetHeight() });
            if (!frame.Positions || !frame.TexCoords || !frame.Normals || !frame.Tangents || !frame.Indices)
            {
                cmd.EndRendering(); // the clear still happens: an empty scene renders black
                return;
            }
            cmd.BindVertexBuffers(0, { frame.Positions, frame.TexCoords, frame.Normals, frame.Tangents }, { 0, 0, 0, 0 });
            cmd.BindIndexBuffer(*frame.Indices, RHI::IndexType::Uint32);
            const RHI::IRHIDescriptorSet& view = services.AllocateForwardViewUniform(MakeForwardViewUniform(frame));
            cmd.BindDescriptorSet(pipeline, 0, view);
            assert(frame.SceneLights && "Forward: the shared phase has not uploaded the scene lights.");
            if (frame.SceneLights)
                cmd.BindDescriptorSet(pipeline, 2, *frame.SceneLights);
            // The sun's shadow: the frame's shadow uniform (unshadowed without a shadow view) and the
            // atlas; then the environment's image-based lighting.
            const RHI::IRHIDescriptorSet& lighting = AllocateLighting(data, services, frame);
            cmd.BindDescriptorSet(pipeline, 3, lighting);
            DrawLitInstances(cmd, lit, frame, frame.OpaqueInstances, false);
            DrawLitInstances(cmd, lit, frame, frame.CutoffInstances, false);

            // Skinned instances after the rigid ones, with the skinning streams bound after the
            // four the rigid pipeline reads and the palette at set 4. The skinned layout's push
            // constants differ, so every set is bound again (the material set by DrawLitInstances).
            if ((CanDrawSkinned(frame) || CanDrawSkinned(frame, frame.SkinnedCutoffInstances)) && frame.SceneLights)
            {
                const RHI::IRHIPipeline& skinnedDoubleSided = services.GetPipeline(EnginePipeline::ForwardSkinnedDoubleSided);
                SidedPipeline            litSkinned(
                    data.CullBackFaces ? services.GetPipeline(EnginePipeline::ForwardSkinned) : skinnedDoubleSided,
                    skinnedDoubleSided);
                const RHI::IRHIPipeline& skinned = litSkinned.Bind(cmd);
                cmd.BindVertexBuffers(0, { frame.Positions, frame.TexCoords, frame.Normals, frame.Tangents, frame.Joints,
                                           frame.Weights },
                                      { 0, 0, 0, 0, 0, 0 });
                cmd.BindDescriptorSet(skinned, 0, view);
                cmd.BindDescriptorSet(skinned, 2, *frame.SceneLights);
                cmd.BindDescriptorSet(skinned, 3, lighting);
                cmd.BindDescriptorSet(skinned, 4, *frame.JointPalette);
                DrawLitInstances(cmd, litSkinned, frame, frame.SkinnedInstances, true);
                DrawLitInstances(cmd, litSkinned, frame, frame.SkinnedCutoffInstances, true);
            }
            cmd.EndRendering();
        }

        // Draws Transparent instances blended in the order given (back to front), each lit as the
        // forward pass lights it. A double-sided one draws its back faces first (through backFaces,
        // which culls the front), then its front faces, so its near side blends over its far side.
        void DrawTransparentInstances(RHI::IRHICommandList& cmd, const RHI::IRHIPipeline& frontFaces,
                                      const RHI::IRHIPipeline& backFaces, const GraphFrameData& frame,
                                      std::span<const HX::RenderInstance> instances, bool skinned)
        {
            const RHI::IRHIPipeline* bound         = &frontFaces;
            uint64_t                 boundMaterial = UINT64_MAX;
            const auto draw = [&](const RHI::IRHIPipeline& pipeline, const HX::RenderInstance& instance)
            {
                if (bound != &pipeline)
                {
                    cmd.BindPipeline(pipeline);
                    bound = &pipeline;
                }
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
            };
            for (const HX::RenderInstance& instance : instances)
            {
                if (instance.MeshIndex >= frame.Meshes.size() || instance.MaterialIndex >= frame.MaterialSets.size()
                    || !frame.MaterialSets[instance.MaterialIndex])
                {
                    continue;
                }
                if (instance.MaterialIndex != boundMaterial)
                {
                    cmd.BindDescriptorSet(*bound, 1, *frame.MaterialSets[instance.MaterialIndex]);
                    boundMaterial = instance.MaterialIndex;
                }
                if (IsDoubleSided(frame, instance))
                    draw(backFaces, instance);
                draw(frontFaces, instance);
            }
        }

        void RecordForwardTransparent(ForwardPassData& data, RHI::IRHICommandList& cmd)
        {
            if (!data.Context)
                return;
            const GraphFrameData& frame = *data.Context->Frame;
            const bool rigid   = !frame.TransparentInstances.empty();
            const bool skinned = CanDrawSkinned(frame, frame.SkinnedTransparentInstances);
            if ((!rigid && !skinned) || !frame.Positions || !frame.TexCoords || !frame.Normals || !frame.Tangents
                || !frame.Indices || !frame.SceneLights)
            {
                return; // nothing to blend: the HDR target is left as Forward and the Skybox made it
            }
            IGraphPassServices& services = *data.Context->Services;
            RHI::IRHITexture&   color    = *data.Graph->GetTexture(data.Color);
            RHI::IRHITexture&   depth    = *data.Graph->GetTexture(data.Depth);

            BeginForwardRendering(cmd, color, depth, false);
            const RHI::IRHIPipeline& pipeline = services.GetPipeline(EnginePipeline::ForwardTransparent);
            cmd.BindPipeline(pipeline);
            cmd.SetViewport({ 0.0f, 0.0f, static_cast<float>(color.GetWidth()),
                              static_cast<float>(color.GetHeight()), 0.0f, 1.0f });
            cmd.SetScissor({ 0, 0, color.GetWidth(), color.GetHeight() });
            const RHI::IRHIDescriptorSet& view     = services.AllocateForwardViewUniform(MakeForwardViewUniform(frame));
            const RHI::IRHIDescriptorSet& lighting = AllocateLighting(data, services, frame);
            if (rigid)
            {
                cmd.BindVertexBuffers(0, { frame.Positions, frame.TexCoords, frame.Normals, frame.Tangents }, { 0, 0, 0, 0 });
                cmd.BindIndexBuffer(*frame.Indices, RHI::IndexType::Uint32);
                cmd.BindDescriptorSet(pipeline, 0, view);
                cmd.BindDescriptorSet(pipeline, 2, *frame.SceneLights);
                cmd.BindDescriptorSet(pipeline, 3, lighting);
                DrawTransparentInstances(cmd, pipeline, services.GetPipeline(EnginePipeline::ForwardTransparentBackFaces),
                                         frame, frame.TransparentInstances, false);
            }
            // Skinned instances after the rigid ones, every set bound again for the skinned layout.
            if (skinned)
            {
                const RHI::IRHIPipeline& skinnedPipeline = services.GetPipeline(EnginePipeline::ForwardTransparentSkinned);
                cmd.BindPipeline(skinnedPipeline);
                cmd.BindVertexBuffers(0, { frame.Positions, frame.TexCoords, frame.Normals, frame.Tangents, frame.Joints,
                                           frame.Weights },
                                      { 0, 0, 0, 0, 0, 0 });
                cmd.BindIndexBuffer(*frame.Indices, RHI::IndexType::Uint32);
                cmd.BindDescriptorSet(skinnedPipeline, 0, view);
                cmd.BindDescriptorSet(skinnedPipeline, 2, *frame.SceneLights);
                cmd.BindDescriptorSet(skinnedPipeline, 3, lighting);
                cmd.BindDescriptorSet(skinnedPipeline, 4, *frame.JointPalette);
                DrawTransparentInstances(cmd, skinnedPipeline,
                                         services.GetPipeline(EnginePipeline::ForwardTransparentSkinnedBackFaces), frame,
                                         frame.SkinnedTransparentInstances, true);
            }
            cmd.EndRendering();
        }

        void BuildForwardTransparent(RenderGraphRuntime& graph, PassInvocation& invocation)
        {
            graph.AddPass<ForwardPassData>(invocation.GetName(),
                [&](RGPassBuilder& pass, ForwardPassData& data)
                {
                    data.Depth = invocation.GetSlot("depth");
                    pass.DepthReadOnly(data.Depth);
                    data.ShadowMap = invocation.GetSlot("shadowMap");
                    pass.SampleTexture(data.ShadowMap);
                    data.Color   = pass.ColorTarget(invocation.GetSlot("color"));
                    data.Graph   = &graph;
                    data.Context = graph.GetFrameContext();
                    invocation.SetSlot("color", data.Color);
                },
                [](ForwardPassData& data, RHI::IRHICommandList& cmd) { RecordForwardTransparent(data, cmd); });
        }

        void BuildForward(RenderGraphRuntime& graph, PassInvocation& invocation)
        {
            // The instantiator has already checked the value is a flag; a C++ caller may omit it.
            const std::optional<bool> cull = ResolveFlag(invocation.GetParameter("cullBackFaces").value_or("true"));
            assert(cull && "Forward: cullBackFaces must be 'true' or 'false'.");

            graph.AddPass<ForwardPassData>(invocation.GetName(),
                [&](RGPassBuilder& pass, ForwardPassData& data)
                {
                    data.Depth = invocation.GetSlot("depth");
                    pass.DepthReadOnly(data.Depth);
                    data.ShadowMap = invocation.GetSlot("shadowMap");
                    pass.SampleTexture(data.ShadowMap);
                    data.Color         = pass.ColorTarget(invocation.GetSlot("color"));
                    data.CullBackFaces = cull.value_or(true);
                    data.Graph         = &graph;
                    data.Context       = graph.GetFrameContext();
                    invocation.SetSlot("color", data.Color);
                },
                [](ForwardPassData& data, RHI::IRHICommandList& cmd) { RecordForward(data, cmd); });
        }
    }

    PassTypeInfo GetForwardPassType()
    {
        return { { "color", "depth", "shadowMap" }, { { "cullBackFaces", PassParameterKind::Flag } }, &BuildForward };
    }

    PassTypeInfo GetForwardTransparentPassType()
    {
        return { { "color", "depth", "shadowMap" }, {}, &BuildForwardTransparent };
    }
}
