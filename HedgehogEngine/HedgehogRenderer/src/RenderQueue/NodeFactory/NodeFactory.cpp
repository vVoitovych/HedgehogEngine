#include "NodeFactory.hpp"
#include "NodeConfigUtils.hpp"

#include "../IRenderNode.hpp"
#include "../Nodes/DepthPrepassNode.hpp"
#include "../Nodes/ForwardPassNode.hpp"
#include "../Nodes/ShadowmapNode.hpp"
#include "../Nodes/TransitionNode.hpp"
#include "../../ResourceManager/ResourceManager.hpp"

#include "Logger/api/Logger.hpp"

#include <cassert>
#include <filesystem>

namespace Renderer
{
    std::unordered_map<std::string, NodeFactory::NodeCreator>& NodeFactory::GetRegistry()
    {
        static std::unordered_map<std::string, NodeCreator> s_Registry;
        return s_Registry;
    }

    void NodeFactory::Register(const std::string& shaderFilename, NodeCreator creator)
    {
        GetRegistry()[shaderFilename] = std::move(creator);
    }

    static void RegisterBuiltinNodes()
    {
        static bool s_Done = false;
        if (s_Done) return;
        s_Done = true;

        NodeFactory::Register("DepthPrepass.shader",
            [](const NodeConfig& cfg, RHI::IRHIDevice& d, ResourceManager& rm)
            { return std::make_unique<DepthPrepassNode>(cfg, d, rm); });

        NodeFactory::Register("ForwardPass.shader",
            [](const NodeConfig& cfg, RHI::IRHIDevice& d, ResourceManager& rm)
            { return std::make_unique<ForwardPassNode>(cfg, d, rm); });

        NodeFactory::Register("ShadowmapPass.shader",
            [](const NodeConfig& cfg, RHI::IRHIDevice& d, ResourceManager& rm)
            { return std::make_unique<ShadowmapNode>(cfg, d, rm); });
    }

    NodeFactory::NodeFactory(RHI::IRHIDevice& device, ResourceManager& resourceManager)
        : m_Device(device)
        , m_ResourceManager(resourceManager)
    {
        RegisterBuiltinNodes();
    }

    std::unique_ptr<IRenderNode> NodeFactory::Create(const NodeConfig& config) const
    {
        if (config.m_Type == "RenderPass")
            return CreateRenderPassNode(config);

        if (config.m_Type == "Transition")
            return CreateTransitionNode(config);

        LOGERROR("NodeFactory: unknown node type '", config.m_Type, "' in pass '", config.m_Name, "'");
        assert(false && "Unknown node type in .rq file");
        return nullptr;
    }

    std::unique_ptr<IRenderNode> NodeFactory::CreateRenderPassNode(const NodeConfig& config) const
    {
        const std::string filename = std::filesystem::path(config.m_Shader).filename().string();

        auto& registry = GetRegistry();
        const auto it  = registry.find(filename);
        if (it == registry.end())
        {
            LOGERROR("NodeFactory: no concrete node registered for shader '",
                     config.m_Shader, "' in pass '", config.m_Name, "'");
            assert(false && "No concrete node registered for this shader in NodeFactory");
            return nullptr;
        }

        return it->second(config, m_Device, m_ResourceManager);
    }

    std::unique_ptr<IRenderNode> NodeFactory::CreateTransitionNode(const NodeConfig& config) const
    {
        if (config.m_Resource.empty())
        {
            LOGERROR("NodeFactory: Transition pass '", config.m_Name, "' has no resource field");
            assert(false && "Transition node missing resource name");
            return nullptr;
        }

        if (!m_ResourceManager.HasTexture(config.m_Resource))
        {
            LOGERROR("NodeFactory: unknown resource '", config.m_Resource,
                     "' in Transition pass '", config.m_Name, "'");
            assert(false && "Unknown resource name in .rq Transition node");
            return nullptr;
        }

        const RHI::ImageLayout fromLayout = ParseImageLayout(config.m_FromLayout);
        const RHI::ImageLayout toLayout   = ParseImageLayout(config.m_ToLayout);

        return std::make_unique<TransitionNode>(config.m_Resource, fromLayout, toLayout);
    }

} // namespace Renderer
