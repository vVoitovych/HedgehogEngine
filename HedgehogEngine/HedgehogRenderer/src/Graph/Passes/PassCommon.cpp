#include "PassCommon.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHITexture.hpp"

namespace Renderer
{
    void DrawOpaqueInstances(RHI::IRHICommandList& cmd, const RHI::IRHIPipeline& pipeline,
                             const GraphFrameData& frame)
    {
        if (!frame.Positions || !frame.Indices)
            return;
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
}
