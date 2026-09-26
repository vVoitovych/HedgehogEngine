#pragma once

#include "HedgehogRenderer/Graph/UiCallback.hpp"
#include "HedgehogRenderer/Views/View.hpp"

#include "RHI/api/RHITypes.hpp"

#include <cstdint>
#include <functional>
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
    class IResourceCatalog;
}

namespace HX
{
    struct RenderScene;
}

namespace HR
{
    class ResourceRegistry;
}

namespace RHI
{
    class IRHIDevice;
    class IRHITexture;
}

namespace Renderer
{
    class FrameRenderer;
    class RHIContext;

    // Vulkan validation-layer diagnostics, safe to query even after the Renderer
    // is destroyed (teardown errors such as leaked objects are still counted).
    bool     AreValidationLayersEnabled();
    uint32_t GetValidationErrorCount();
    uint32_t GetValidationWarningCount();

    // Handed to the application once, while the Renderer is constructed: the device, and the format
    // the result view's Ui pass records into. It is what the application needs to create its UI
    // renderer (RHIImGui) and to wait for the GPU before destroying it, while the
    // renderer never knows the UI. The device lives until Cleanup.
    struct RendererDevice
    {
        RHI::IRHIDevice& Device;
        RHI::Format      PresentFormat = RHI::Format::Undefined;
    };
    using DeviceReadyCallback = std::function<void(const RendererDevice&)>;

    // The frame path (RENDERING.md section 9): extract a RenderScene, SyncResources, RenderFrame. It
    // renders the application's views and one per enabled camera in the scene, and presents the
    // highest-priority view that targets main (RENDERING.md section 8).
    class Renderer
    {
    public:
        Renderer(HW::Window& window, const FS::FileSystemManager& fileSystem,
                 const DeviceReadyCallback& onDeviceReady = {});
        ~Renderer();

        Renderer(const Renderer&)            = delete;
        Renderer& operator=(const Renderer&) = delete;

        void Cleanup();

        // Uploads new or changed meshes and materials.
        void SyncResources(HedgehogEngine::IResourceCatalog& catalog);

        // ui records into the target of any view whose graph has a Ui pass (the result view).
        void RenderFrame(const HX::RenderScene& scene, const HedgehogSettings::Settings& settings,
                         const UiCallback& ui = {});

        // CPU frame statistics (RenderFrame's total and each render-graph pass's record time), used
        // by the Editor --benchmark mode. Capture is off unless explicitly begun.
        void BeginFrameStatsCapture();
        void EndFrameStatsCaptureAndLogReport();

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

        std::unique_ptr<RHIContext>           m_RHIContext;
        std::unique_ptr<HR::ResourceRegistry> m_Resources; // mesh and material GPU data
        std::unique_ptr<FrameRenderer>        m_FrameRenderer;
    };
}
