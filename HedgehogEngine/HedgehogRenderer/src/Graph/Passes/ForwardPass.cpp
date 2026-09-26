#include "ForwardPass.hpp"

#include "PassCommon.hpp"

#include "HedgehogRenderer/Graph/GraphAssetVocabulary.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHITexture.hpp"

#include <cassert>
#include <cstdint>
#include <optional>

namespace Renderer
{
    namespace
    {
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
            if (!frame.Positions || !frame.TexCoords || !frame.Normals || !frame.Indices)
            {
                cmd.EndRendering(); // the clear still happens: an empty scene renders black
                return;
            }
            cmd.BindVertexBuffers(0, { frame.Positions, frame.TexCoords, frame.Normals }, { 0, 0, 0 });
            cmd.BindIndexBuffer(*frame.Indices, RHI::IndexType::Uint32);
            cmd.BindDescriptorSet(pipeline, 0, services.AllocateForwardViewUniform(MakeForwardViewUniform(frame)));
            assert(frame.SceneLights && "Forward: the shared phase has not uploaded the scene lights.");
            if (frame.SceneLights)
                cmd.BindDescriptorSet(pipeline, 2, *frame.SceneLights);

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

    PassTypeInfo GetForwardPassType()
    {
        return { { "color", "depth", "shadowMap" }, { { "cullBackFaces", PassParameterKind::Flag } }, &BuildForward };
    }
}
