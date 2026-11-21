#pragma once

#include "HedgehogRenderer/Graph/MeshDrawRange.hpp"

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
        std::vector<HX::RenderInstance> Terrain;            // GraphFrameData::TerrainInstances
    };

    // Draws every instance at LOD 0 when true: flipped locally to measure what LODs save.
    inline constexpr bool FORCE_LOD0 = false;

    // An instance whose bounding sphere covers less than this fraction of the view's height draws
    // LOD 1; each later level at half the fraction of the one before (LOD 2 below 1/8, LOD 3 below 1/16).
    inline constexpr float LOD1_SCREEN_FRACTION = 0.25f;

    // The level of detail of a mesh with lodCount levels whose world bounds are seen from eye through
    // proj (a perspective or orthographic projection): its bounding sphere's projected height as a
    // fraction of the view's, against LOD1_SCREEN_FRACTION halved per level, clamped to the coarsest.
    // 0 when the eye is inside the sphere, for a single level, and with FORCE_LOD0.
    [[nodiscard]] uint32_t SelectMeshLod(const HM::AABB& worldBounds, const HM::Vector3& eye,
                                         const HM::Matrix4x4& proj, uint32_t lodCount);

    // What a view chooses its instances' levels of detail by: its eye, its projection and the frame's
    // meshes (MeshDrawRange::LodCount), by RenderInstance::MeshIndex.
    struct LodView
    {
        HM::Vector3                    Eye;
        HM::Matrix4x4                  Proj;
        std::span<const MeshDrawRange> Meshes;
    };

    // SelectMeshLod for one instance; 0 for a mesh index outside lod.Meshes.
    [[nodiscard]] uint32_t SelectInstanceLod(const HX::RenderInstance& instance, const LodView& lod);

    // Keeps the instances on a layer in layerMask whose world bounds intersect the frustum of
    // viewProj (a Vulkan-style projection times the view matrix). Editor-layer instances go to
    // Overlay, terrain tiles (TerrainIndex >= 0) to Terrain; every other one by its material's alpha
    // mode (materials[MaterialIndex]; Opaque for a material without an entry) and whether it has a
    // joint palette. With lod, each kept instance gets its SelectInstanceLod for that view (the
    // overlay keeps LOD 0: the selection's silhouette is the mesh's own).
    void CullViewInstances(std::span<const HX::RenderInstance> instances, uint32_t layerMask,
                           const HM::Matrix4x4& viewProj, ViewInstances& out,
                           std::span<const MaterialDrawInfo> materials = {}, const LodView* lod = nullptr);

    // Orders instances back to front from eye, by the distance to their world bounds' centre (ties
    // by SourceId, so the order is stable from frame to frame): the blending order of transparent
    // ones. Sorts in place, allocating nothing.
    void SortBackToFront(std::span<HX::RenderInstance> instances, const HM::Vector3& eye);

    // The instances that may cast the sun's shadow: all but the editor layer's and those that do not
    // cast (RenderInstance::CastsShadows). What the shared shadow pass chooses its casters from,
    // before the caster mask (SharedPhase.hpp).
    void CollectSceneInstances(std::span<const HX::RenderInstance> instances, std::vector<HX::RenderInstance>& out);
}
