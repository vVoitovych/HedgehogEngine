#pragma once

namespace YAML { class Node; }

namespace Renderer
{
    // Optional interface for nodes that need to parse node-specific YAML config.
    // RenderNodeManager calls SetConfig() after node creation, before Setup().
    class IConfigurableNode
    {
    public:
        virtual ~IConfigurableNode() = default;
        virtual void SetConfig(const YAML::Node& entry) = 0;
    };
}
