#pragma once

#include "HedgehogRenderer/Graph/UiCallback.hpp"
#include "HedgehogRenderer/Views/View.hpp"

#include <cstdint>
#include <memory>

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
    class IResourceCatalog;
}

namespace HX
{
    struct RenderScene;
}

namespace RHI
{
    class IRHIGuiBackend;
    class IRHITexture;
}

namespace Renderer
{
    class FrameRenderer;
    class RHIContext;
    class ThreadContext;
    class ResourceManager;
    class RenderQueue;

    // Vulkan validation-layer diagnostics, safe to query even after the Renderer
    // is destroyed (teardown errors such as leaked objects are still counted).
    bool     AreValidationLayersEnabled();
    uint32_t GetValidationErrorCount();
    uint32_t GetValidationWarningCount();

    // Which frame paths a Renderer builds. The editor builds both and picks one per frame with
    // RenderingSettings::GetUseRenderGraph. RenderGraphOnly builds no legacy pass: DrawFrame and
    // the legacy scene-view texture are unavailable. The renderer never creates a UI context on
    // either path; the application owns its UI and records it through a UiCallback.
    enum class RendererPaths
    {
        LegacyAndRenderGraph,
        RenderGraphOnly,
    };

    class Renderer
    {
    public:
        Renderer(HW::Window& window,
                 const HedgehogSettings::Settings& settings,
                 const FS::FileSystemManager& fileSystem,
                 RendererPaths paths = RendererPaths::LegacyAndRenderGraph);
        ~Renderer();

        Renderer(const Renderer&)            = delete;
        Renderer& operator=(const Renderer&) = delete;

        void Cleanup();

        // The legacy path. ui records into the legacy colour buffer.
        void  DrawFrame(const HedgehogEngine::FrameData& frameData,
                        HedgehogEngine::IResourceCatalog& catalog,
                        HedgehogSettings::Settings&       settings,
                        const UiCallback&                 ui = {});
        float GetAspectRatio() const;
        // The legacy path's scene image, for the application's scene panel, and its size.
        const RHI::IRHITexture& GetSceneViewTexture() const;
        void                    SetSceneViewSize(uint32_t width, uint32_t height);

        // A GUI renderer (the RHI's ImGui backend) for the colour target a path records its UI
        // into: the legacy colour buffer, or the swapchain for the render graph's result view.
        // Create it after the application's ImGui context; it is tied to that target's format.
        [[nodiscard]] std::unique_ptr<RHI::IRHIGuiBackend> CreateGuiBackend(bool forRenderGraph) const;

        // Blocks until the GPU is idle, e.g. before replacing a GUI backend.
        void WaitIdle() const;

        // CPU frame statistics (per render pass + total DrawFrame), used by
        // the Editor --benchmark mode. Capture is off unless explicitly begun.
        void BeginFrameStatsCapture();
        void EndFrameStatsCaptureAndLogReport();

        // ── The render-graph path (RENDERING.md) ──────────────────────────────────────────
        // Runs instead of DrawFrame when RenderingSettings::GetUseRenderGraph() is on; both
        // paths are built, so the flag can change between frames. It renders the views below
        // (and one per enabled camera in the scene) and presents VIEWPORT_TARGET. There is no
        // UI pass yet: the ImGui frame BeginGui started is discarded.

        // The target a view renders into to be presented, sized to the window.
        static constexpr const char* VIEWPORT_TARGET = "viewport";

        // Uploads new or changed meshes and materials. DrawFrame does this itself.
        void SyncResources(HedgehogEngine::IResourceCatalog& catalog);

        // ui records into the target of any view whose graph has a Ui pass (the result view).
        void RenderFrame(const HX::RenderScene& scene, const HedgehogSettings::Settings& settings,
                         const UiCallback& ui = {});

        // Application views (the editor's), never reconciled against the scene's cameras.
        [[nodiscard]] ViewId CreateView(ViewDesc desc);
        [[nodiscard]] bool   UpdateView(ViewId id, ViewDesc desc);
        void                 DestroyView(ViewId id);

        // Redirects the view derived from a camera (by its SourceId) to other targets, leaving the
        // camera untouched: the editor draws the game camera into its game panel.
        void SetCameraTargetOverride(uint64_t cameraSourceId, std::vector<std::string> targets);

        // Named render targets (RENDERING.md section 4). Declare between frames; a resize applies
        // at the end of the frame, and 0x0 makes the target zero-area, which drops its views.
        RenderTargetResult DeclareTarget(const RenderTargetDesc& desc);
        RenderTargetResult ResizeTarget(std::string_view name, uint32_t width, uint32_t height);
        // The texture behind a declared target; nullptr while unknown or zero-area. It changes only
        // at the end of a frame that resized the target, so a UI texture id made from it stays valid
        // until then.
        RHI::IRHITexture* GetTargetTexture(std::string_view name) const;

        // How many render-graph passes the last RenderFrame executed, across every view: a hidden
        // (zero-area) view contributes none.
        size_t GetLastFramePassCount() const;

    private:
        HW::Window& m_Window;

        std::unique_ptr<RHIContext>      m_RHIContext;
        std::unique_ptr<ThreadContext>   m_ThreadContext;
        std::unique_ptr<ResourceManager> m_ResourceManager;
        std::unique_ptr<RenderQueue>     m_RenderQueue;
        std::unique_ptr<FrameRenderer>   m_FrameRenderer;

        uint32_t m_DesiredSceneW = 0;
        uint32_t m_DesiredSceneH = 0;
    };
}
