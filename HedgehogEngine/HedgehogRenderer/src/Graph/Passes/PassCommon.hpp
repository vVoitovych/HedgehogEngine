#pragma once

#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"
#include "HedgehogRenderer/Graph/PassInvocation.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"

#include <cstdint>
#include <span>
#include <utility>

namespace RHI
{
    class IRHICommandList;
    class IRHIPipeline;
    class IRHITexture;
}

// What the engine pass types (one pair of files each in this folder) share: the pass data they keep
// between setup and execute, and the recording helpers more than one of them uses.
namespace Renderer
{
    // The Forward and Gizmo passes: a colour target drawn against the view's depth. Only Forward
    // reads CullBackFaces and samples ShadowMap.
    struct ForwardPassData
    {
        RGTexture                Color{};
        RGTexture                Depth{};
        RGTexture                ShadowMap{};
        RenderGraphRuntime*      Graph         = nullptr;
        const GraphFrameContext* Context       = nullptr;
        bool                     CullBackFaces = true;
    };

    // What a single-target engine pass keeps between setup and execute: its target, and pointers
    // to the runtime and frame context. Pointers and handles only - the arena never destroys it.
    struct TargetPassData
    {
        RGTexture                Target{};
        RenderGraphRuntime*      Graph   = nullptr;
        const GraphFrameContext* Context = nullptr;
    };

    // Whether an instance's material is double-sided (GraphFrameData::Materials); false for a
    // material without draw info.
    bool IsDoubleSided(const GraphFrameData& frame, const HX::RenderInstance& instance);

    // A pipeline and its twin without back-face culling: the one an instance draws with follows its
    // material (IsDoubleSided), bound only when it differs from the one bound last. The twins share
    // a layout, so the sets bound before stay bound. Make it with the twin twice to draw every
    // instance double-sided (a forward pass with cullBackFaces: false).
    class SidedPipeline
    {
    public:
        SidedPipeline(const RHI::IRHIPipeline& single, const RHI::IRHIPipeline& doubleSided)
            : m_Single(&single), m_DoubleSided(&doubleSided)
        {
        }

        // Binds the single-sided pipeline, the one the caller binds its sets with.
        const RHI::IRHIPipeline& Bind(RHI::IRHICommandList& cmd);
        // The pipeline instance draws with, bound first when it is not the one bound.
        const RHI::IRHIPipeline& Use(RHI::IRHICommandList& cmd, const GraphFrameData& frame,
                                     const HX::RenderInstance& instance);

    private:
        const RHI::IRHIPipeline* m_Single;
        const RHI::IRHIPipeline* m_DoubleSided;
        const RHI::IRHIPipeline* m_Bound = nullptr;
    };

    // Binds the positions and draws each rigid instance with its model matrix as the push constant,
    // through pipeline. Instances whose mesh has no draw range are skipped, and nothing is drawn
    // before any geometry has been uploaded.
    void DrawRigidInstances(RHI::IRHICommandList& cmd, SidedPipeline& pipeline, const GraphFrameData& frame,
                            std::span<const HX::RenderInstance> instances);

    // Binds the shared geometry and draws every opaque instance with its model matrix as the
    // push constant. Instances whose mesh has no draw range are skipped, and nothing is drawn
    // before any geometry has been uploaded.
    void DrawOpaqueInstances(RHI::IRHICommandList& cmd, const RHI::IRHIPipeline& pipeline,
                             const GraphFrameData& frame);

    // Whether the frame has skinned instances and everything drawing them needs: the skinning
    // vertex streams and the joint palette.
    bool CanDrawSkinned(const GraphFrameData& frame);
    // The same for a list of skinned instances.
    bool CanDrawSkinned(const GraphFrameData& frame, std::span<const HX::RenderInstance> instances);

    // Binds the positions and skinning streams (the skinned depth pipeline's vertex description)
    // and draws every skinned instance with its model matrix and palette offset as the push
    // constants. The caller binds pipeline and its sets first. Instances whose mesh has no draw
    // range are skipped.
    void DrawSkinnedInstances(RHI::IRHICommandList& cmd, const RHI::IRHIPipeline& pipeline,
                              const GraphFrameData& frame);
    // The same for a list of skinned instances, each through pipeline's twin its material asks for.
    void DrawSkinnedInstances(RHI::IRHICommandList& cmd, SidedPipeline& pipeline, const GraphFrameData& frame,
                              std::span<const HX::RenderInstance> instances);

    // Starts depth-only dynamic rendering into target, cleared to 1. Returns its size.
    uint32_t BeginDepthRendering(RHI::IRHICommandList& cmd, RHI::IRHITexture& target);

    // Declares a pass that writes only the depth texture bound to slot, and records execute.
    template<typename ExecuteFn>
    void AddDepthOnlyPass(RenderGraphRuntime& graph, PassInvocation& invocation, const char* slot,
                          ExecuteFn&& execute)
    {
        graph.AddPass<TargetPassData>(invocation.GetName(),
            [&](RGPassBuilder& pass, TargetPassData& data)
            {
                data.Target  = pass.DepthTarget(invocation.GetSlot(slot));
                data.Graph   = &graph;
                data.Context = graph.GetFrameContext();
                invocation.SetSlot(slot, data.Target);
            },
            std::forward<ExecuteFn>(execute));
    }
}
