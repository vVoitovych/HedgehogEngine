#pragma once

#include <cstdint>

namespace HedgehogEngine
{
    struct FrameData;
}

namespace RHI
{
    class IRHIDevice;
}

namespace HedgehogSettings
{
    class Settings;
}

namespace Renderer
{
    class ResourceManager;
    class RenderContext;

    class IRenderNode
    {
    public:
        virtual ~IRenderNode() = default;

        virtual void Execute(RenderContext& ctx)       = 0;
        virtual void Cleanup(RHI::IRHIDevice& device)  = 0;

        virtual void OnWindowResize(RHI::IRHIDevice& device,
                                    const ResourceManager& resourceManager) {}
        virtual void OnSceneViewResize(RHI::IRHIDevice& device,
                                       const ResourceManager& resourceManager) {}
        virtual void OnSettingsChanged(RHI::IRHIDevice& device,
                                       const HedgehogSettings::Settings& settings,
                                       const ResourceManager& resourceManager) {}
        virtual void UpdateData(const HedgehogEngine::FrameData& frame,
                                uint32_t frameIndex,
                                const HedgehogSettings::Settings& settings) {}

        virtual void  BeginFrame()                  {}
        virtual void  DiscardFrame()                {}
        virtual void* GetSceneViewTextureId() const { return nullptr; }

        bool IsEnabled() const  { return m_Enabled; }
        void SetEnabled(bool v) { m_Enabled = v; }

    private:
        bool m_Enabled = true;
    };
}
