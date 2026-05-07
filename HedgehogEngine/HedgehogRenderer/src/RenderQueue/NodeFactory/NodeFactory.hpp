#pragma once

#include "NodeConfig.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace RHI
{
    class IRHIDevice;
}

namespace Renderer
{
    class IRenderNode;
    class ResourceManager;

    // Creates IRenderNode instances from NodeConfig descriptors loaded by RenderQueueLoader.
    // RenderPass nodes are dispatched via a registration table; Transition is an explicit branch.
    class NodeFactory
    {
    public:
        using NodeCreator = std::function<std::unique_ptr<IRenderNode>(
            const NodeConfig&, RHI::IRHIDevice&, ResourceManager&)>;

        NodeFactory(RHI::IRHIDevice& device, ResourceManager& resourceManager);

        std::unique_ptr<IRenderNode> Create(const NodeConfig& config) const;

        static void Register(const std::string& shaderFilename, NodeCreator creator);

    private:
        static std::unordered_map<std::string, NodeCreator>& GetRegistry();

        std::unique_ptr<IRenderNode> CreateRenderPassNode(const NodeConfig& config) const;
        std::unique_ptr<IRenderNode> CreateTransitionNode(const NodeConfig& config) const;

        RHI::IRHIDevice& m_Device;
        ResourceManager& m_ResourceManager;
    };

} // namespace Renderer
