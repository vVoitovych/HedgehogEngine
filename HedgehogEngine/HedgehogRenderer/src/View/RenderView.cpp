#include "RenderView.hpp"

namespace Renderer
{
    RenderView::RenderView(std::string name, RenderResourceLedger& ledger)
        : m_Name(std::move(name))
        , m_Pipeline(ledger)
    {
        m_Pipeline.GetGraph().SetScope(m_Name);
    }

    RenderView::~RenderView()
    {
    }

    void RenderView::Compile(RHI::IRHIDevice& device)
    {
        // Frame-graph resources (SwapchainRelative) don't apply to a view; pass 0 for those —
        // a view graph only ever declares ViewRelative and Fixed transients.
        m_Pipeline.Compile(device, 0, 0, m_DesiredWidth, m_DesiredHeight);
        m_CompiledWidth  = m_DesiredWidth;
        m_CompiledHeight = m_DesiredHeight;
    }

    std::string RenderView::OutputColorName() const
    {
        return m_Name + "/" + GraphResourceNames::VIEW_COLOR;
    }
}
