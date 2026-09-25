#include "HedgehogSettings/api/RenderingSettings.hpp"

namespace HedgehogSettings
{
    bool RenderingSettings::GetUseRenderGraph() const
    {
        return m_UseRenderGraph;
    }

    void RenderingSettings::SetUseRenderGraph(bool useRenderGraph)
    {
        m_UseRenderGraph = useRenderGraph;
    }
}
