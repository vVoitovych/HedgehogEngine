#pragma once

#include "IRenderNode.hpp"

#include <memory>

namespace Renderer
{
    class ShadowmapPass;

    class ShadowmapNode : public IRenderNode
    {
    public:
        ShadowmapNode(RHI::IRHIDevice& device,
                      const HedgehogSettings::Settings& settings,
                      const ResourceManager& resourceManager);

        void Execute(NodeContext& ctx) override;
        void Cleanup(RHI::IRHIDevice& device) override;

        void UpdateData(const HedgehogEngine::FrameData& frame,
                        uint32_t frameIndex,
                        const HedgehogSettings::Settings& settings) override;

        void OnSettingsChanged(RHI::IRHIDevice& device,
                               const HedgehogSettings::Settings& settings,
                               const ResourceManager& resourceManager) override;

    private:
        std::unique_ptr<ShadowmapPass> m_Pass;
    };
}
