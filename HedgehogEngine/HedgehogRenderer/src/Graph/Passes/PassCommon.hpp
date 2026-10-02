#pragma once

#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"
#include "HedgehogRenderer/Graph/PassInvocation.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"

#include <cstdint>
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
    // reads CullBackFaces.
    struct ForwardPassData
    {
        RGTexture                Color{};
        RGTexture                Depth{};
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

    // Binds the shared geometry and draws every opaque instance with its model matrix as the
    // push constant. Instances whose mesh has no draw range are skipped, and nothing is drawn
    // before any geometry has been uploaded.
    void DrawOpaqueInstances(RHI::IRHICommandList& cmd, const RHI::IRHIPipeline& pipeline,
                             const GraphFrameData& frame);

    // Whether the frame has skinned instances and everything drawing them needs: the skinning
    // vertex streams and the joint palette.
    bool CanDrawSkinned(const GraphFrameData& frame);

    // Binds the positions and skinning streams (the skinned depth pipeline's vertex description)
    // and draws every skinned instance with its model matrix and palette offset as the push
    // constants. The caller binds pipeline and its sets first. Instances whose mesh has no draw
    // range are skipped.
    void DrawSkinnedInstances(RHI::IRHICommandList& cmd, const RHI::IRHIPipeline& pipeline,
                              const GraphFrameData& frame);

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
