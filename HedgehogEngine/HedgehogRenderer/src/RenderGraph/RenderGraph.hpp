#pragma once

#include "RenderGraphTypes.hpp"
#include "GraphResourceRegistry.hpp"

#include <string>
#include <vector>

namespace HedgehogEngine
{
    struct FrameData;
}

namespace RHI
{
    class IRHIDevice;
    class IRHIBuffer;
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

        // Calls Setup()+ApplyYAMLBindings() on every node, creates graph-declared GPU
        // resources, builds the texture registry, validates bindings, and plans barriers.
        void Compile(RHI::IRHIDevice& device, const ResourceManager& resourceManager);

        // Calls Cleanup() on each node. Does NOT free node memory — caller must do that
        // via RenderNodeManager::DestroyAll() after this returns.
        void Execute(RenderContext& ctx);
        void Cleanup(RHI::IRHIDevice& device);

        const TextureRegistry& GetTextureRegistry() const { return m_TextureRegistry; }

        // Resource declarations — called by CreateGPUResourceNode::Setup().
        void DeclareGraphTexture(const GraphTextureDesc& desc);
        void DeclareGraphBuffer(const GraphBufferDesc& desc);

        // Look up a graph-declared buffer by name (textures are in the TextureRegistry).
        const RHI::IRHIBuffer* GetBuffer(const std::string& name) const;

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
        void ValidateBindings() const;
        void BuildBarrierPlan();

        std::vector<IRenderNode*>              m_Nodes;
        TextureRegistry                        m_TextureRegistry;
        std::vector<std::vector<BarrierEntry>> m_BarrierPlan;
        GraphResourceRegistry                  m_GraphResources;
    };
}
