#pragma once

#include "RenderGraphTypes.hpp"

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

        // Add a non-owning pointer; ownership stays with RenderNodeManager.
        void AddNode(IRenderNode* node);

        // Calls Setup() on every node then builds the texture registry from ResourceManager.
        void Compile(const ResourceManager& resourceManager);

        // Calls Cleanup() on each node. Does NOT free node memory — caller must do that
        // via RenderNodeManager::DestroyAll() after this returns.
        void Execute(RenderContext& ctx);
        void Cleanup(RHI::IRHIDevice& device);

        const TextureRegistry& GetTextureRegistry() const { return m_TextureRegistry; }

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
        void BuildTextureRegistry(const ResourceManager& resourceManager);
        void BuildBarrierPlan();

        std::vector<IRenderNode*>              m_Nodes;
        TextureRegistry                        m_TextureRegistry;
        std::vector<std::vector<BarrierEntry>> m_BarrierPlan;
    };
}
