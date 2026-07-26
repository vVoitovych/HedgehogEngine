#pragma once

#include "HedgehogMath/api/Matrix.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace HW
{
    class Window;
}

namespace HedgehogSettings
{
    class Settings;
}

namespace FS
{
    class FileSystemManager;
}

namespace HedgehogEngine
{
    struct FrameData;
    struct CameraData;
    class IResourceCatalog;
}

namespace HR
{
    class ResourceRegistry;
}

namespace Renderer
{
    class RHIContext;
    class ThreadContext;
    class ViewRegistry;
    class RenderPipeline;
    class RenderResourceLedger;
    class PassResourceCache;
    class RenderPassRegistry;
    class FrameStats;

    // Vulkan validation-layer diagnostics, safe to query even after the Renderer
    // is destroyed (teardown errors such as leaked objects are still counted).
    bool     AreValidationLayersEnabled();
    uint32_t GetValidationErrorCount();
    uint32_t GetValidationWarningCount();

    using ViewHandle = uint32_t;
    inline constexpr ViewHandle INVALID_VIEW = 0;

    // What a view renders with. PipelineAsset is a virtual path to a `stage: view` .rgq asset
    // (e.g. "engine://HedgehogEngine/HedgehogRenderer/assets/Graphs/scene_view.rgq") — see
    // RenderGraphLoader / workflow/current-plan.md, ".rgq schema (version 1)".
    struct ViewDesc
    {
        std::string Name;
        std::string PipelineAsset;
        uint32_t    Width  = 1;
        uint32_t    Height = 1;
    };

    class Renderer
    {
    public:
        Renderer(HW::Window& window,
                 const HedgehogSettings::Settings& settings,
                 const FS::FileSystemManager& fileSystem);
        ~Renderer();

        Renderer(const Renderer&)            = delete;
        Renderer& operator=(const Renderer&) = delete;

        void Cleanup();

        void  BeginGui();
        void  DrawFrame(const HedgehogEngine::FrameData& frameData,
                        HedgehogEngine::IResourceCatalog& catalog,
                        HedgehogSettings::Settings&       settings);

        // Loads and compiles a `stage: frame` / `stage: composition` .rgq asset as the shared
        // once-per-frame stage that runs before every view, or the once-per-frame stage that runs
        // after every view, respectively. Replaces any previously set pipeline of the same kind.
        // Must both be called (in either order relative to each other, but SetCompositionPipeline
        // should be called after every CreateView so its composition sees the final view set — see
        // CreateView) before the first DrawFrame(); DrawFrame no-ops if either is unset. Returns
        // false + LOGERROR (asset unreadable, malformed, or a pass failed to construct) without
        // crashing or disturbing whatever pipeline was already set — the caller decides whether
        // that's fatal.
        [[nodiscard]] bool SetFramePipeline(const std::string& assetPath);
        [[nodiscard]] bool SetCompositionPipeline(const std::string& assetPath);

        // Loads and compiles a `stage: view` .rgq asset as a new view. Returns nullopt + LOGERROR
        // on any load/construction failure, leaving no partial state behind. Every view should be
        // created — and SetCompositionPipeline called — before the first DrawFrame(); creating a
        // view afterward does not update an already-compiled composition pipeline's import list
        // (full dynamic recomposition is out of scope for now — a future `.rgq` reload naturally
        // rebuilds whatever it affects).
        [[nodiscard]] std::optional<ViewHandle> CreateView(const ViewDesc& desc);
        void                                    DestroyView(ViewHandle handle);
        [[nodiscard]] std::optional<ViewHandle> FindView(const std::string& name) const;

        // Which view's camera the frame graph's ShadowmapPass fits cascades to. Purely
        // functional — unrelated to render/profiling order (see CreateView's stats-suffix rule).
        void SetMainView(ViewHandle handle);

        // Desired size, applied at end-of-frame (see DrawFrame's deferred-resize sweep) so the
        // current frame's already-submitted ImGui draw data never references a freed descriptor.
        void SetViewSize(ViewHandle handle, uint32_t width, uint32_t height);
        // Whether this view's graph runs this frame; a disabled view's geometry passes are skipped.
        void SetViewEnabled(ViewHandle handle, bool enabled);
        // camera == nullopt clears the view's camera for this frame (see RenderView::SetCamera) —
        // called unconditionally, e.g. every frame with frameData.GameCamera as-is, so a view
        // that loses its camera reliably falls back to a clear instead of a stale image.
        void SetViewCamera(ViewHandle handle, const std::optional<HedgehogEngine::CameraData>& camera);
        // World matrix of the selected entity for this view's gizmo overlay (views without a
        // GizmoPass ignore this); nullopt = none.
        void SetViewGizmo(ViewHandle handle, const std::optional<HM::Matrix4x4>& worldMatrix);

        void* GetViewTextureId(ViewHandle handle) const;
        float GetViewAspectRatio(ViewHandle handle) const;

        // CPU frame statistics (per render pass + total DrawFrame), used by
        // the Editor --benchmark mode. Capture is off unless explicitly begun.
        void BeginFrameStatsCapture();
        void EndFrameStatsCaptureAndLogReport();

    private:
        HW::Window&                       m_Window;
        const HedgehogSettings::Settings& m_Settings;
        const FS::FileSystemManager&      m_FileSystem;

        std::unique_ptr<RHIContext>    m_RHIContext;
        std::unique_ptr<ThreadContext> m_ThreadContext;

        std::unique_ptr<HR::ResourceRegistry> m_ResourceRegistry;
        std::unique_ptr<PassResourceCache>    m_PassResourceCache;
        std::unique_ptr<RenderResourceLedger> m_Ledger;
        std::unique_ptr<RenderPassRegistry>   m_PassRegistry;

        std::unique_ptr<ViewRegistry> m_ViewRegistry;
        ViewHandle                    m_MainView = INVALID_VIEW;

        std::unique_ptr<RenderPipeline> m_FramePipeline;
        std::unique_ptr<RenderPipeline> m_CompositionPipeline;

        std::unique_ptr<FrameStats> m_FrameStats;
    };
}
