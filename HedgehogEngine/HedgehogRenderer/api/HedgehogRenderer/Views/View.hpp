#pragma once

#include "HedgehogRenderer/Targets/RenderTargetRegistry.hpp"

#include "HedgehogExtract/api/RenderScene.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Renderer
{
    using ViewId = uint64_t;
    inline constexpr ViewId INVALID_VIEW_ID = 0;

    // Where a view came from (RENDERING.md section 1). Never both for the same thing.
    enum class ViewOrigin
    {
        Derived,     // from an enabled CameraComponent, via RenderScene; reconciled every build
        Application, // from ViewManager::CreateView (the editor's own views); never reconciled
    };

    // The part of the target a view draws into, normalized to [0, 1].
    struct ViewRect
    {
        float X      = 0.0f;
        float Y      = 0.0f;
        float Width  = 1.0f;
        float Height = 1.0f;
    };

    // A render request (RENDERING.md section 1): not an entity, never serialized.
    struct ViewDesc
    {
        std::optional<HX::RenderCamera> Camera;    // absent for a pure composite (the editor's result view)
        std::vector<std::string>        Targets;   // ordered: Targets[i] binds the graph's output slot i
        // Targets this view samples (e.g. the result view reads the scene and game panels). A view
        // runs after every view that writes one of them (ViewOrdering.hpp). Derived views read
        // nothing until materials can reference render targets.
        std::vector<std::string>        Reads;
        std::string                     GraphName;
        uint32_t                        LayerMask = 0xFFFFFFFFu;
        ViewRect                        Viewport;
        int32_t                         Priority  = 0;
        bool                            IsEnabled = true;
    };

    // One view that survived this frame's build, with every target resolved (all Status == Ok).
    struct View
    {
        ViewId                            Id     = INVALID_VIEW_ID;
        ViewOrigin                        Origin = ViewOrigin::Application;
        uint64_t                          SourceId = 0; // the camera entity, for Derived views only
        ViewDesc                          Desc;
        std::vector<ResolvedRenderTarget> ResolvedTargets;
    };

    enum class ViewDropReason
    {
        Disabled,
        NoTargets,
        UnknownTarget,
        ZeroAreaTarget,
        Cycle,          // part of a render-target dependency cycle (ViewOrdering.hpp)
    };

    // A view skipped at build time (RENDERING.md section 3.2): dropped here, never later.
    struct DroppedView
    {
        ViewId         Id     = INVALID_VIEW_ID;
        ViewDropReason Reason = ViewDropReason::Disabled;
        std::string    Message; // names the view and, for target problems, the target
    };
}
