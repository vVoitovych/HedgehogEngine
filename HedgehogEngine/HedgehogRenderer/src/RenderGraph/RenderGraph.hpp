#pragma once

#include <memory>
#include <vector>

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
    class IRenderNode;
    class RenderContext;
    class ResourceManager;

    class RenderGraph
    {
    public:
        RenderGraph()  = default;
        ~RenderGraph() = default;

        RenderGraph(const RenderGraph&)            = delete;
        RenderGraph& operator=(const RenderGraph&) = delete;

        void AddNode(std::unique_ptr<IRenderNode> node);

        // Validates the node list. Future phases will topologically sort nodes
        // and build the barrier plan here.
        void Compile();

        void Execute(RenderContext& ctx);
        void Cleanup(RHI::IRHIDevice& device);

        // Lifecycle events — forwarded to all nodes via their virtual overrides.
        void BeginFrame();
        void DiscardFrame();
        void* GetSceneViewTextureId() const;

        void UpdateData(const HedgehogEngine::FrameData&    frame,
                        uint32_t                            frameIndex,
                        const HedgehogSettings::Settings&   settings);

        void OnWindowResize(RHI::IRHIDevice& device, const ResourceManager& resourceManager);
        void OnSceneViewResize(RHI::IRHIDevice& device, const ResourceManager& resourceManager);
        void OnSettingsChanged(RHI::IRHIDevice& device,
                               const HedgehogSettings::Settings& settings,
                               const ResourceManager& resourceManager);

    private:
        std::vector<std::unique_ptr<IRenderNode>> m_Nodes;
    };
}
