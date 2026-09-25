#pragma once

#include "HedgehogSettingsApi.hpp"

namespace HedgehogSettings
{
    // Which frame path the renderer runs. The render graph path (Renderer::RenderFrame) is
    // being brought up alongside the legacy fixed passes (Renderer::DrawFrame); until it
    // replaces them, it runs only when this is switched on, as `rendering.use_render_graph`
    // in the engine settings file. Switching it rebuilds no GPU resource, so it does not mark
    // the settings dirty.
    class RenderingSettings
    {
    public:
        HEDGEHOG_SETTINGS_API RenderingSettings() = default;

        ~RenderingSettings() = default;
        RenderingSettings(const RenderingSettings&) = delete;
        RenderingSettings(RenderingSettings&&) = delete;
        RenderingSettings& operator=(const RenderingSettings&) = delete;
        RenderingSettings& operator=(RenderingSettings&&) = delete;

        HEDGEHOG_SETTINGS_API bool GetUseRenderGraph() const;
        HEDGEHOG_SETTINGS_API void SetUseRenderGraph(bool useRenderGraph);

    private:
        bool m_UseRenderGraph = false;
    };
}
