#pragma once

#include "RenderGraph/RenderGraphTypes.hpp"

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
    class RenderGraph;

    class IRenderNode
    {
    public:
        virtual ~IRenderNode() = default;

        // Called by RenderGraph::Compile(). Override to declare resource slots
        // (read/write dependencies) that future phases use for barrier planning.
        virtual void Setup(RenderGraph& graph) {}

        virtual void Execute(RenderContext& ctx)       = 0;
        virtual void Cleanup(RHI::IRHIDevice& device)  = 0;

        virtual const RenderNodeDesc& GetDesc() const { return m_Desc; }

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

    protected:
        RenderNodeDesc m_Desc;

    private:
        bool m_Enabled = true;
    };
}
