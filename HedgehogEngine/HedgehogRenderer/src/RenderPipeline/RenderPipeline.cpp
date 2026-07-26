#include "RenderPipeline.hpp"

namespace Renderer
{
    RenderPipeline::RenderPipeline(RenderResourceLedger& ledger)
        : m_Graph(ledger)
    {
    }

    RenderPipeline::~RenderPipeline()
    {
    }

    void RenderPipeline::AddPass(std::unique_ptr<IRenderPass> pass)
    {
        m_Graph.AddPass(pass.get());
        m_OwnedPasses.push_back(std::move(pass));
    }

    void RenderPipeline::Compile(RHI::IRHIDevice& device, uint32_t swapchainWidth, uint32_t swapchainHeight,
                                 uint32_t viewWidth, uint32_t viewHeight)
    {
        m_Graph.Compile(device, swapchainWidth, swapchainHeight, viewWidth, viewHeight);
    }

    void RenderPipeline::Cleanup(RHI::IRHIDevice& device)
    {
        for (auto& pass : m_OwnedPasses)
            pass->Cleanup(device);
        m_Graph.Cleanup(device);
        m_OwnedPasses.clear();
    }

    void RenderPipeline::BeginFrame()
    {
        for (auto& pass : m_OwnedPasses)
            pass->BeginFrame();
    }

    void RenderPipeline::DiscardFrame()
    {
        for (auto& pass : m_OwnedPasses)
            pass->DiscardFrame();
    }

    void RenderPipeline::NotifyViewsChanged(const std::vector<std::string>& viewOutputNames)
    {
        for (auto& pass : m_OwnedPasses)
            pass->OnViewsChanged(viewOutputNames);
    }

    void RenderPipeline::NotifySettingsDirty(RHI::IRHIDevice& device, const HedgehogSettings::Settings& settings)
    {
        for (auto& pass : m_OwnedPasses)
            pass->OnSettingsDirty(m_Graph, device, settings);
    }

    void* RenderPipeline::GetViewTextureId(const std::string& viewOutputName) const
    {
        for (auto& pass : m_OwnedPasses)
        {
            if (void* id = pass->GetViewTextureId(viewOutputName))
                return id;
        }
        return nullptr;
    }
}
