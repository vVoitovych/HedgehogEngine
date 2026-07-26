#pragma once

#include "HedgehogCommon/api/Frame/FrameData.hpp"

#include "RHI/api/RHITypes.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace RHI
{
    class IRHIDevice;
    class IRHICommandList;
    class IRHISwapchain;
    class IRHIFence;
    class IRHISemaphore;
}

namespace HedgehogSettings
{
    class Settings;
}

namespace HR
{
    class ResourceRegistry;
}

namespace Renderer
{
    // Size classes a graph transient texture can belong to. A single Invalidate(class) call
    // recreates every texture in that class and rebuilds exactly the dependent passes.
    enum class SizeClass
    {
        SwapchainRelative,
        ViewRelative,  // relative to the owning graph's RenderGraph::SetViewSize (a view-stage graph only)
        Fixed,
    };

    // Which stage of the frame a graph belongs to. A frame graph runs once per frame before every
    // view (e.g. shadowmap); a view graph is one instance per registered RenderView; a composition
    // graph runs once per frame after every view (GUI + present). Validated against a loaded
    // .rgq's declared `stage:` field once the YAML loader exists (see workflow/current-plan.md).
    enum class GraphStage
    {
        Frame,
        View,
        Composition,
    };

    struct GraphTextureDesc
    {
        SizeClass         TextureSizeClass = SizeClass::SwapchainRelative;
        RHI::Format       Format           = RHI::Format::Undefined;
        RHI::TextureUsage Usage            = RHI::TextureUsage::None;
        // Only used when TextureSizeClass == Fixed.
        uint32_t          FixedWidth  = 0;
        uint32_t          FixedHeight = 0;
    };

    using ResourceHandle = uint32_t;
    inline constexpr ResourceHandle INVALID_RESOURCE_HANDLE = static_cast<ResourceHandle>(-1);

    // Canonical local names for the graph's transient textures (Design decisions,
    // workflow/current-plan.md). Frame/composition-stage assets scope these under "shared/";
    // view-stage assets scope them under their own view's name.
    namespace GraphResourceNames
    {
        inline constexpr const char* VIEW_COLOR   = "viewColor";
        inline constexpr const char* VIEW_DEPTH   = "viewDepth";
        inline constexpr const char* SHADOW_DEPTH = "shadowDepth";
        inline constexpr const char* GUI_COLOR    = "guiColor";
    }

    // Editor-only, per-view frame-varying data set by the application each frame. Every non-gizmo
    // pass ignores SelectedGizmo, and a game build never populates it — see workflow/current-plan.md,
    // "Editor-only data".
    struct ViewFrameData
    {
        HedgehogEngine::CameraData   Camera;
        bool                         FrustumCull = false;
        std::optional<HM::Matrix4x4> SelectedGizmo;

        // False when the application hasn't supplied a camera for this view this frame (e.g. the
        // game view with no primary CameraComponent in the scene). DepthPrePass/ForwardPass draw
        // an empty bucket (clear-only) rather than the shared draw list when this is false, so the
        // view still gets a fresh clear every frame instead of showing a stale image — see
        // workflow/current-plan.md, "no-primary-camera fallback".
        bool HasCamera = false;
    };

    // Frame-varying inputs handed to every pass's Update()/Execute(). Resource lookups
    // (transient textures) go through RenderGraph::GetTexture(handle), reached via
    // CreateFramebuffers(device, graph) at compile/invalidate time — passes bind the
    // resulting textures/framebuffers once rather than looking them up every frame.
    struct RenderGraphContext
    {
        const HedgehogEngine::FrameData* FrameData  = nullptr;
        uint32_t                         FrameIndex = 0;

        RHI::IRHIDevice*      Device      = nullptr;
        RHI::IRHICommandList* CommandList = nullptr;
        RHI::IRHISwapchain*   Swapchain   = nullptr;
        uint32_t               BackBufferIndex = 0;

        RHI::IRHIFence*     Fence                   = nullptr;
        RHI::IRHISemaphore* ImageAvailableSemaphore  = nullptr;
        RHI::IRHISemaphore* RenderFinishedSemaphore  = nullptr;

        HR::ResourceRegistry*             ResourceRegistry = nullptr;
        const HedgehogSettings::Settings* Settings         = nullptr;

        // Non-null only while executing a view-stage graph; null for frame/composition passes.
        const ViewFrameData* View = nullptr;
    };
}
