#pragma once

#include "HedgehogMath/api/AABB.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include <span>

namespace ECS
{
    class ECS;
}

namespace HedgehogEngine
{
    class RenderSystem;
    class LightSystem;
    class CameraSystem;
    class UiSystem;
}

namespace HX
{
    struct RenderScene;

    // The one type that reads the ECS read-only and writes a RenderScene (RENDERING.md
    // section 3.1). After extraction, nothing downstream touches the ECS again.
    //
    // Deliberately single-threaded and stateless — safe to construct per call.
    class SceneExtractor
    {
    public:
        // Does not call outScene.Clear() itself: callers reuse one RenderScene across frames
        // and decide when to clear it (see RenderScene::Clear).
        //
        // meshLocalBounds is indexed by MeshComponent::MeshIndex (MeshBoundsCache::GetBounds); an
        // instance whose mesh it does not cover gets a unit cube as its local bounds.
        //
        // An instance with an AnimatorComponent whose palette is filled is skinned: its palette is
        // appended to outScene.JointMatrices (RenderInstance::PaletteOffset/JointCount) and its local
        // bounds are inflated (SKINNED_BOUNDS_MARGIN in SceneExtractor.cpp).
        void Extract(
            const ECS::ECS&                     ecs,
            const HedgehogEngine::RenderSystem&  renderSystem,
            const HedgehogEngine::LightSystem&   lightSystem,
            const HedgehogEngine::CameraSystem&  cameraSystem,
            RenderScene&                         outScene,
            std::span<const HM::AABB>            meshLocalBounds = {}) const;

        // Lays out every enabled canvas of uiSystem for a target targetSize pixels across and appends
        // its quads to outScene.Ui, canvases by UiCanvasComponent::SortOrder (then entity id), each
        // canvas's elements in hierarchy order, parents before children. An element is a child with a
        // UiRectComponent, resolved inside its parent's rect; one hidden (IsVisible false) or on the
        // editor layer (a RenderComponent with HX::EDITOR_LAYER) emits nothing, nor do its children,
        // and so does a child without a UiRectComponent or with a UiCanvasComponent of its own. An
        // element's UiImageComponent draws its rect, tinted by its UiButtonComponent's state; texture
        // paths are numbered in outScene.UiTextures. Sets outScene.UiTargetSize.
        void ExtractUi(const ECS::ECS& ecs, const HedgehogEngine::UiSystem& uiSystem, const HM::Vector2& targetSize,
                       RenderScene& outScene) const;

    private:
        void ExtractInstances(const ECS::ECS& ecs, const HedgehogEngine::RenderSystem& renderSystem,
                               std::span<const HM::AABB> meshLocalBounds, RenderScene& outScene) const;
        void ExtractLights(const ECS::ECS& ecs, const HedgehogEngine::LightSystem& lightSystem,
                            RenderScene& outScene) const;
        void ExtractCameras(const ECS::ECS& ecs, const HedgehogEngine::CameraSystem& cameraSystem,
                             RenderScene& outScene) const;
    };
}
