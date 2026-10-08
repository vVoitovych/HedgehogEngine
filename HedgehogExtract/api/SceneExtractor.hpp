#pragma once

#include "HedgehogMath/api/AABB.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include <cstddef>
#include <cstdint>
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
    class PhysicsSystem;
    class FontContainer;
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
        // Collider wireframe colours (RGBA8, red lowest, picked in sRGB; HX::DebugLineVertex).
        static constexpr uint32_t PHYSICS_DEBUG_STATIC_COLOR    = 0xff80b280; // grey-green: no rigid body, or a static one
        static constexpr uint32_t PHYSICS_DEBUG_DYNAMIC_COLOR   = 0xff2896ff; // orange
        static constexpr uint32_t PHYSICS_DEBUG_KINEMATIC_COLOR = 0xffff9650; // blue
        static constexpr uint32_t PHYSICS_DEBUG_TRIGGER_COLOR   = 0xff3ce6ff; // yellow, whatever the body
        // Segments of a sphere's circle or a capsule's end circle; a capsule's half arc takes half.
        static constexpr size_t PHYSICS_DEBUG_CIRCLE_SEGMENTS = 24;

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
        // paths are numbered in outScene.UiTextures. Sets outScene.UiTargetSize. With fonts, an
        // element's UiTextComponent draws its text over the image, laid out in the element's rect with
        // its font baked at its FontSize in target pixels (fonts->FindOrBake, so a new font or size is
        // baked here), the fonts it uses numbered in outScene.UiFonts; without fonts, or with a font
        // that does not load, text draws nothing.
        void ExtractUi(const ECS::ECS& ecs, const HedgehogEngine::UiSystem& uiSystem, const HM::Vector2& targetSize,
                       RenderScene& outScene, HedgehogEngine::FontContainer* fonts = nullptr) const;

        // Appends the wireframe of every collider of physicsSystem (Collider + Transform) to
        // outScene.DebugLines in world space, shaped as its body is: through the entity's ObjMatrix,
        // its world axes made unit length and their lengths its scale, offset by the collider's
        // Center. A box gives its 12 edges; a sphere (Radius times the largest scale) three great
        // circles; a capsule along local +Z (Radius and Height / 2 - Radius times the largest scale)
        // its two end circles, four side lines and four half arcs. Coloured by trigger, then body
        // kind (the PHYSICS_DEBUG_*_COLOR constants). It reads components only, so it works in Edit
        // mode too, and allocates nothing once the lines' storage has grown.
        void ExtractPhysicsDebug(const ECS::ECS& ecs, const HedgehogEngine::PhysicsSystem& physicsSystem,
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
