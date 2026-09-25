#pragma once

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
        float GetAspectRatio() const;
        void* GetSceneViewTextureId() const;
        void  SetSceneViewSize(uint32_t width, uint32_t height);

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

        void RenderFrame(const HX::RenderScene& scene, const HedgehogSettings::Settings& settings);

        // Application views (the editor's), never reconciled against the scene's cameras.
        [[nodiscard]] ViewId CreateView(ViewDesc desc);
        [[nodiscard]] bool   UpdateView(ViewId id, ViewDesc desc);
        void                 DestroyView(ViewId id);

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
