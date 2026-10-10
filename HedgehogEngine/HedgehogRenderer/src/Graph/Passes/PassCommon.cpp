#include "PassCommon.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHITexture.hpp"

#include <algorithm>

namespace Renderer
{
    bool IsDoubleSided(const GraphFrameData& frame, const HX::RenderInstance& instance)
    {
        return instance.MaterialIndex < frame.Materials.size() && frame.Materials[instance.MaterialIndex].DoubleSided;
    }

    const RHI::IRHIPipeline& SidedPipeline::Bind(RHI::IRHICommandList& cmd)
    {
        cmd.BindPipeline(*m_Single);
        m_Bound = m_Single;
        return *m_Single;
    }

    const RHI::IRHIPipeline& SidedPipeline::Use(RHI::IRHICommandList& cmd, const GraphFrameData& frame,
                                                const HX::RenderInstance& instance)
    {
        const RHI::IRHIPipeline* wanted = IsDoubleSided(frame, instance) ? m_DoubleSided : m_Single;
        if (wanted != m_Bound)
        {
            cmd.BindPipeline(*wanted);
            m_Bound = wanted;
        }
        return *wanted;
    }

    void DrawRigidInstances(RHI::IRHICommandList& cmd, SidedPipeline& pipeline, const GraphFrameData& frame,
                            std::span<const HX::RenderInstance> instances)
    {
        if (!frame.Positions || !frame.Indices)
            return;
        cmd.BindVertexBuffers(0, { frame.Positions }, { 0 });
        cmd.BindIndexBuffer(*frame.Indices, RHI::IndexType::Uint32);
        for (const HX::RenderInstance& instance : instances)
        {
            if (instance.MeshIndex >= frame.Meshes.size())
                continue;
            const MeshDrawRange&     mesh  = frame.Meshes[instance.MeshIndex];
            const RHI::IRHIPipeline& bound = pipeline.Use(cmd, frame, instance);
            cmd.PushConstants(bound, RHI::ShaderStage::Vertex, 0, 16 * sizeof(float), instance.WorldMatrix.GetBuffer());
            cmd.DrawIndexed(mesh.IndexCount, 1, mesh.FirstIndex, static_cast<int32_t>(mesh.VertexOffset), 0);
        }
    }

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
        return CanDrawSkinned(frame, frame.SkinnedInstances);
    }

    bool CanDrawSkinned(const GraphFrameData& frame, std::span<const HX::RenderInstance> instances)
    {
        return !instances.empty() && frame.JointPalette && frame.Positions && frame.Joints && frame.Weights && frame.Indices;
    }

    void DrawSkinnedInstances(RHI::IRHICommandList& cmd, SidedPipeline& pipeline, const GraphFrameData& frame,
                              std::span<const HX::RenderInstance> instances)
    {
        cmd.BindVertexBuffers(0, { frame.Positions, frame.Joints, frame.Weights }, { 0, 0, 0 });
        cmd.BindIndexBuffer(*frame.Indices, RHI::IndexType::Uint32);
        for (const HX::RenderInstance& instance : instances)
        {
            if (instance.MeshIndex >= frame.Meshes.size())
                continue;
            const MeshDrawRange&       mesh      = frame.Meshes[instance.MeshIndex];
            const RHI::IRHIPipeline&   bound     = pipeline.Use(cmd, frame, instance);
            const SkinnedPushConstants constants = MakeSkinnedPushConstants(instance);
            cmd.PushConstants(bound, RHI::ShaderStage::Vertex, 0, sizeof(constants), &constants);
            cmd.DrawIndexed(mesh.IndexCount, 1, mesh.FirstIndex, static_cast<int32_t>(mesh.VertexOffset), 0);
        }
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
