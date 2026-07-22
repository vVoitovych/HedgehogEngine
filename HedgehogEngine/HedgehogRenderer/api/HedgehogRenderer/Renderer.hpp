#pragma once

#include "HedgehogMath/api/Matrix.hpp"

#include <cstdint>
#include <memory>
#include <optional>

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

namespace Renderer
{
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
        float GetGameViewAspectRatio() const;
        void* GetSceneViewTextureId() const;
        void  SetSceneViewSize(uint32_t width, uint32_t height);

        void* GetGameViewTextureId() const;
        void  SetGameViewSize(uint32_t width, uint32_t height);
        // Whether the game viewport is currently shown; when false its geometry pass is skipped.
        void  SetGameViewVisible(bool visible);

        // World matrix of the selected entity for the scene-view gizmo overlay; nullopt = none.
        void  SetSelectedGizmo(const std::optional<HM::Matrix4x4>& worldMatrix);

        // CPU frame statistics (per render pass + total DrawFrame), used by
        // the Editor --benchmark mode. Capture is off unless explicitly begun.
        void BeginFrameStatsCapture();
        void EndFrameStatsCaptureAndLogReport();

    private:
        HW::Window& m_Window;

        std::unique_ptr<RHIContext>      m_RHIContext;
        std::unique_ptr<ThreadContext>   m_ThreadContext;
        std::unique_ptr<ResourceManager> m_ResourceManager;
        std::unique_ptr<RenderQueue>     m_RenderQueue;

        uint32_t m_DesiredSceneW = 0;
        uint32_t m_DesiredSceneH = 0;
        uint32_t m_DesiredGameW  = 0;
        uint32_t m_DesiredGameH  = 0;
        bool     m_GameViewVisible = false;
        std::optional<HM::Matrix4x4> m_SelectedGizmo;
    };
}
