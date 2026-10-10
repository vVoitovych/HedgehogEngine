#pragma once

#include "HedgehogCommon/api/Resource/IResourceCatalog.hpp"
#include "HedgehogExtract/api/RenderScene.hpp"
#include "HedgehogMath/api/Matrix.hpp"

#include <cstdint>
#include <span>
#include <vector>

// Per-view culling (RENDERING.md section 3.3): which of the frame's instances one view draws. Reads
// only RenderScene data: each instance's layer and precomputed world bounds, and its material's
// draw info.
namespace Renderer
{
    // What the passes need of a material to choose how to draw it, indexed by
    // RenderInstance::MaterialIndex (ResourceRegistry::GetMaterialDrawInfos).
    struct MaterialDrawInfo
    {
        HedgehogEngine::MaterialAlphaMode AlphaMode   = HedgehogEngine::MaterialAlphaMode::Opaque;
        bool                              DoubleSided = false;
    };

    // One view's instances, split by what draws them: rigid or skinned (JointCount > 0), and by
    // their material's alpha mode. Reused across frames: CullViewInstances clears, never shrinks, so
    // a steady-state frame allocates nothing.
    struct ViewInstances
    {
        std::vector<HX::RenderInstance> Opaque;             // GraphFrameData::OpaqueInstances
        std::vector<HX::RenderInstance> Skinned;            // GraphFrameData::SkinnedInstances
        std::vector<HX::RenderInstance> Cutoff;             // GraphFrameData::CutoffInstances
        std::vector<HX::RenderInstance> SkinnedCutoff;      // GraphFrameData::SkinnedCutoffInstances
        std::vector<HX::RenderInstance> Transparent;        // GraphFrameData::TransparentInstances
        std::vector<HX::RenderInstance> SkinnedTransparent; // GraphFrameData::SkinnedTransparentInstances
        std::vector<HX::RenderInstance> Overlay;            // GraphFrameData::OverlayInstances: the editor layer
    };

    // Keeps the instances on a layer in layerMask whose world bounds intersect the frustum of
    // viewProj (a Vulkan-style projection times the view matrix). Editor-layer instances go to
    // Overlay; every other one by its material's alpha mode (materials[MaterialIndex]; Opaque for a
    // material without an entry) and whether it has a joint palette.
    void CullViewInstances(std::span<const HX::RenderInstance> instances, uint32_t layerMask,
                           const HM::Matrix4x4& viewProj, ViewInstances& out,
                           std::span<const MaterialDrawInfo> materials = {});

    // The instances any view could draw as scene geometry: all but the editor layer's. What the
    // shared shadow pass chooses its casters from, before the caster mask (SharedPhase.hpp).
    void CollectSceneInstances(std::span<const HX::RenderInstance> instances, std::vector<HX::RenderInstance>& out);
}
