#pragma once

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

namespace HR
{
    class ResourceRegistry;
}

namespace Renderer
{
    class RHIContext;
    class ThreadContext;
    class RenderGraph;
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

    private:
        HW::Window& m_Window;

        std::unique_ptr<RHIContext>            m_RHIContext;
        std::unique_ptr<ThreadContext>         m_ThreadContext;
        std::unique_ptr<RenderGraph>           m_Graph;
        std::unique_ptr<HR::ResourceRegistry>  m_ResourceRegistry;
        std::unique_ptr<RenderQueue>           m_RenderQueue;

        uint32_t m_DesiredSceneW = 0;
        uint32_t m_DesiredSceneH = 0;
    };
}
