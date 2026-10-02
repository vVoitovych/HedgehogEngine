#include "PassCommon.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHITexture.hpp"

#include <algorithm>

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

    SkinnedPushConstants MakeSkinnedPushConstants(const HX::RenderInstance& instance)
    {
        SkinnedPushConstants constants;
        std::copy_n(instance.WorldMatrix.GetBuffer(), 16, constants.Model);
        constants.PaletteOffset = instance.PaletteOffset;
        return constants;
    }

    bool CanDrawSkinned(const GraphFrameData& frame)
    {
        return !frame.SkinnedInstances.empty() && frame.JointPalette && frame.Positions && frame.Joints
            && frame.Weights && frame.Indices;
    }

    void DrawSkinnedInstances(RHI::IRHICommandList& cmd, const RHI::IRHIPipeline& pipeline,
                              const GraphFrameData& frame)
    {
        cmd.BindVertexBuffers(0, { frame.Positions, frame.Joints, frame.Weights }, { 0, 0, 0 });
        cmd.BindIndexBuffer(*frame.Indices, RHI::IndexType::Uint32);
        for (const HX::RenderInstance& instance : frame.SkinnedInstances)
        {
            if (instance.MeshIndex >= frame.Meshes.size())
                continue;
            const MeshDrawRange&       mesh      = frame.Meshes[instance.MeshIndex];
            const SkinnedPushConstants constants = MakeSkinnedPushConstants(instance);
            cmd.PushConstants(pipeline, RHI::ShaderStage::Vertex, 0, sizeof(constants), &constants);
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
