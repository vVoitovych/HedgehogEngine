#pragma once

#include "../IRenderNode.hpp"

#include <memory>

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
    class ShadowmapPass;

    class ShadowmapNode final : public IRenderNode
    {
    public:
        ShadowmapNode(RHI::IRHIDevice& device,
                      const HedgehogSettings::Settings& settings,
                      const ResourceManager& resourceManager);
        ~ShadowmapNode() override;

        void Render(RenderContext& ctx) override;
        void Cleanup(RHI::IRHIDevice& device) override;

        void UpdateData(const HedgehogEngine::FrameData& frame,
                        uint32_t frameIndex,
                        const HedgehogSettings::Settings& settings) override;

        void OnUpdateResources(RHI::IRHIDevice& device,
                               const HedgehogSettings::Settings& settings,
                               const ResourceManager& rm) override;

    private:
        std::unique_ptr<ShadowmapPass> m_Pass;
    };

} // namespace Renderer
