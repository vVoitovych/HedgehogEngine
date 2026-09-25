#include "HedgehogRenderer/Graph/EnginePassTypes.hpp"

#include "HedgehogRenderer/Graph/GraphAssetVocabulary.hpp"
#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"
#include "HedgehogRenderer/Graph/ShadowCascades.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHITexture.hpp"

#include <cassert>
#include <cstdint>

namespace Renderer
{
    namespace
    {
        struct TargetPassData
        {
            RGTexture Target{};
        };

        struct ForwardPassData
        {
            RGTexture                Color{};
            RGTexture                Depth{};
            RenderGraphRuntime*      Graph         = nullptr;
            const GraphFrameContext* Context       = nullptr;
            bool                     CullBackFaces = true;
        };

        // Declares a pass whose only effect is writing one slot as a depth or color target.
        template<RGTexture (RGPassBuilder::*Write)(RGTexture)>
        void BuildSingleTargetPass(RenderGraphRuntime& graph, PassInvocation& invocation, const char* slot)
        {
            graph.AddPass<TargetPassData>(invocation.GetName(),
                [&](RGPassBuilder& pass, TargetPassData& data)
                {
                    data.Target = (pass.*Write)(invocation.GetSlot(slot));
                    invocation.SetSlot(slot, data.Target);
                },
                [](TargetPassData&, RHI::IRHICommandList&) {});
        }

        // What a depth-only engine pass keeps between setup and execute: its target, and pointers
        // to the runtime and frame context. Pointers and handles only - the arena never destroys it.
        struct DepthOnlyPassData
        {
            RGTexture                Target{};
            RenderGraphRuntime*      Graph   = nullptr;
            const GraphFrameContext* Context = nullptr;
        };

        // Binds the shared geometry and draws every opaque instance with its model matrix as the
        // push constant. Instances whose mesh has no draw range are skipped.
        void DrawOpaqueInstances(RHI::IRHICommandList& cmd, const RHI::IRHIPipeline& pipeline,
                                 const GraphFrameData& frame)
        {
            cmd.BindVertexBuffers(0, { frame.Positions }, { 0 });
            cmd.BindIndexBuffer(*frame.Indices, RHI::IndexType::Uint32);
            for (const HX::RenderInstance& instance : frame.OpaqueInstances)
            {
                if (instance.MeshIndex >= frame.Meshes.size())
                    continue;
                const MeshDrawRange& mesh = frame.Meshes[instance.MeshIndex];
                cmd.PushConstants(pipeline, RHI::ShaderStage::Vertex, 0, 16 * sizeof(float),
                                  instance.WorldMatrix.GetBuffer());
                cmd.DrawIndexed(mesh.IndexCount, 1, mesh.FirstIndex, static_cast<int32_t>(mesh.VertexOffset), 0);
            }
        }

        // Starts depth-only dynamic rendering into target, cleared to 1. Returns its size.
        uint32_t BeginDepthRendering(RHI::IRHICommandList& cmd, RHI::IRHITexture& target)
        {
            RHI::RenderingAttachment depth;
            depth.Texture                    = &target;
            depth.LoadOp                     = RHI::LoadOp::Clear;
            depth.StoreOp                    = RHI::StoreOp::Store;
            depth.Clear.IsDepth              = true;
            depth.Clear.DepthStencil         = { 1.0f, 0 };

            RHI::RenderingInfo info;
            info.DepthAttachment = depth;
            info.Width           = target.GetWidth();
            info.Height          = target.GetHeight();
            cmd.BeginRendering(info);
            return target.GetWidth();
        }

        template<typename ExecuteFn>
        void AddDepthOnlyPass(RenderGraphRuntime& graph, PassInvocation& invocation, const char* slot,
                              ExecuteFn&& execute)
        {
            graph.AddPass<DepthOnlyPassData>(invocation.GetName(),
                [&](RGPassBuilder& pass, DepthOnlyPassData& data)
                {
                    data.Target  = pass.DepthTarget(invocation.GetSlot(slot));
                    data.Graph   = &graph;
                    data.Context = graph.GetFrameContext();
                    invocation.SetSlot(slot, data.Target);
                },
                std::forward<ExecuteFn>(execute));
        }

        void BuildDepthPrepass(RenderGraphRuntime& graph, PassInvocation& invocation)
        {
            AddDepthOnlyPass(graph, invocation, "depth", [](DepthOnlyPassData& data, RHI::IRHICommandList& cmd)
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

        void BuildShadow(RenderGraphRuntime& graph, PassInvocation& invocation)
        {
            AddDepthOnlyPass(graph, invocation, "shadowMap", [](DepthOnlyPassData& data, RHI::IRHICommandList& cmd)
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

        void BuildUi(RenderGraphRuntime& graph, PassInvocation& invocation)
        {
            BuildSingleTargetPass<&RGPassBuilder::ColorTarget>(graph, invocation, "target");
        }

        // Colour cleared to opaque black, drawn against the prepass depth without writing it.
        void BeginForwardRendering(RHI::IRHICommandList& cmd, RHI::IRHITexture& color, RHI::IRHITexture& depth)
        {
            RHI::RenderingAttachment colorAttachment;
            colorAttachment.Texture     = &color;
            colorAttachment.LoadOp      = RHI::LoadOp::Clear;
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

        void RecordForward(ForwardPassData& data, RHI::IRHICommandList& cmd)
        {
            if (!data.Context)
                return;
            const GraphFrameData&    frame    = *data.Context->Frame;
            IGraphPassServices&      services = *data.Context->Services;
            const RHI::IRHIPipeline& pipeline = services.GetPipeline(
                data.CullBackFaces ? EnginePipeline::Forward : EnginePipeline::ForwardDoubleSided);
            RHI::IRHITexture& color = *data.Graph->GetTexture(data.Color);
            RHI::IRHITexture& depth = *data.Graph->GetTexture(data.Depth);

            BeginForwardRendering(cmd, color, depth);
            cmd.BindPipeline(pipeline);
            cmd.SetViewport({ 0.0f, 0.0f, static_cast<float>(color.GetWidth()),
                              static_cast<float>(color.GetHeight()), 0.0f, 1.0f });
            cmd.SetScissor({ 0, 0, color.GetWidth(), color.GetHeight() });
            cmd.BindVertexBuffers(0, { frame.Positions, frame.TexCoords, frame.Normals }, { 0, 0, 0 });
            cmd.BindIndexBuffer(*frame.Indices, RHI::IndexType::Uint32);
            cmd.BindDescriptorSet(pipeline, 0, services.AllocateForwardViewUniform(MakeForwardViewUniform(frame)));

            // Rebind the material set only when it changes between consecutive instances.
            uint64_t boundMaterial = UINT64_MAX;
            for (const HX::RenderInstance& instance : frame.OpaqueInstances)
            {
                if (instance.MeshIndex >= frame.Meshes.size() || instance.MaterialIndex >= frame.MaterialSets.size()
                    || !frame.MaterialSets[instance.MaterialIndex])
                {
                    continue;
                }
                if (instance.MaterialIndex != boundMaterial)
                {
                    cmd.BindDescriptorSet(pipeline, 1, *frame.MaterialSets[instance.MaterialIndex]);
                    boundMaterial = instance.MaterialIndex;
                }
                const MeshDrawRange& mesh = frame.Meshes[instance.MeshIndex];
                cmd.PushConstants(pipeline, RHI::ShaderStage::Vertex, 0, 16 * sizeof(float),
                                  instance.WorldMatrix.GetBuffer());
                cmd.DrawIndexed(mesh.IndexCount, 1, mesh.FirstIndex, static_cast<int32_t>(mesh.VertexOffset), 0);
            }
            cmd.EndRendering();
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
                    pass.SampleTexture(invocation.GetSlot("shadowMap"));
                    data.Color         = pass.ColorTarget(invocation.GetSlot("color"));
                    data.CullBackFaces = cull.value_or(true);
                    data.Graph         = &graph;
                    data.Context       = graph.GetFrameContext();
                    invocation.SetSlot("color", data.Color);
                },
                [](ForwardPassData& data, RHI::IRHICommandList& cmd) { RecordForward(data, cmd); });
        }
    }

    void RegisterEnginePassTypes(PassBuilderRegistry& registry)
    {
        [[maybe_unused]] const bool registered =
            registry.Register("DepthPrepass", { { "depth" }, {}, &BuildDepthPrepass })
            && registry.Register("Shadow", { { "shadowMap" }, {}, &BuildShadow })
            && registry.Register("Forward", { { "color", "depth", "shadowMap" },
                                              { { "cullBackFaces", PassParameterKind::Flag } }, &BuildForward })
            && registry.Register("Ui", { { "target" }, {}, &BuildUi });
        assert(registered && "RegisterEnginePassTypes: an engine pass type was already registered.");
    }
}
