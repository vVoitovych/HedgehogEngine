#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Renderer
{
    class IRenderNode;
    class RenderGraph;

    using RenderNodeHandle                          = uint32_t;
    static constexpr RenderNodeHandle kInvalidNodeHandle = 0;

    struct GraphPreset
    {
        struct Entry
        {
            std::string type;
            std::string instance;
        };
        std::vector<Entry> nodes;
    };

    class RenderNodeManager
    {
    public:
        RenderNodeManager()  = default;
        ~RenderNodeManager() = default;

        RenderNodeManager(const RenderNodeManager&)            = delete;
        RenderNodeManager& operator=(const RenderNodeManager&) = delete;

        // Register a factory for a named node type.
        void RegisterNodeType(const std::string& typeId,
                              std::function<std::unique_ptr<IRenderNode>()> factory);

        // Convenience overload for default-constructible node types.
        template<typename T>
        void RegisterNodeType(const std::string& typeId)
        {
            RegisterNodeType(typeId, []() { return std::make_unique<T>(); });
        }

        // Instantiate a registered type and assign it the given instance name.
        RenderNodeHandle CreateNode(const std::string& typeId, const std::string& instanceName);

        void DestroyNode(RenderNodeHandle handle);
        void DestroyAll();

        IRenderNode* GetNode(RenderNodeHandle handle) const;
        IRenderNode* FindNode(const std::string& instanceName) const;
        void         SetNodeEnabled(RenderNodeHandle handle, bool enabled);

        // Create all nodes in preset order and add them to outGraph (in-code preset).
        void LoadGraphPreset(const GraphPreset& preset, RenderGraph& outGraph);

        // Create all nodes from a YAML file and add them to outGraph.
        // yamlPath is repo-relative, e.g. "/HedgehogEngine/HedgehogRenderer/assets/Graphs/forward_editor.rgq".
        void LoadGraphPreset(const std::string& yamlPath, RenderGraph& outGraph);

    private:
        using NodeFactory = std::function<std::unique_ptr<IRenderNode>()>;

        std::unordered_map<std::string, NodeFactory>                       m_Factories;
        std::unordered_map<RenderNodeHandle, std::unique_ptr<IRenderNode>> m_Instances;
        std::unordered_map<std::string, RenderNodeHandle>                  m_ByName;
        uint32_t                                                           m_NextHandle = 1;
    };
}
