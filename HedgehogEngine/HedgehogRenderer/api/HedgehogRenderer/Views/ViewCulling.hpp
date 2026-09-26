#pragma once

#include "HedgehogExtract/api/RenderScene.hpp"
#include "HedgehogMath/api/Matrix.hpp"

#include <cstdint>
#include <span>
#include <vector>

// Per-view culling (RENDERING.md section 3.3): which of the frame's instances one view draws. Reads
// only RenderScene data: each instance's layer and precomputed world bounds.
namespace Renderer
{
    // One view's instances, split by what draws them. Reused across frames: CullViewInstances
    // clears, never shrinks, so a steady-state frame allocates nothing.
    struct ViewInstances
    {
        std::vector<HX::RenderInstance> Opaque;  // GraphFrameData::OpaqueInstances
        std::vector<HX::RenderInstance> Overlay; // GraphFrameData::OverlayInstances: the editor layer
    };

    // Keeps the instances on a layer in layerMask whose world bounds intersect the frustum of
    // viewProj (a Vulkan-style projection times the view matrix). Editor-layer instances go to
    // Overlay, every other one to Opaque.
    void CullViewInstances(std::span<const HX::RenderInstance> instances, uint32_t layerMask,
                           const HM::Matrix4x4& viewProj, ViewInstances& out);

    // The instances any view could draw as scene geometry: all but the editor layer's. What the
    // shared shadow pass chooses its casters from, before the caster mask (SharedPhase.hpp).
    void CollectSceneInstances(std::span<const HX::RenderInstance> instances, std::vector<HX::RenderInstance>& out);
}
