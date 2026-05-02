#pragma once

#include "NodeConfig.hpp"

#include "RHI/api/RHITypes.hpp"

#include <memory>
#include <string>

namespace RHI
{
    class IRHIDevice;
}

namespace HW
{
    class Window;
}

namespace HedgehogSettings
{
    class Settings;
}

namespace Renderer
{
    class IRenderNode;
    class ResourceManager;

    // Creates IRenderNode instances from NodeConfig descriptors loaded by RenderQueueLoader.
    // Dispatches on NodeConfig::m_Type; for RenderPass nodes, further dispatches on shader filename.
    class NodeFactory
    {
    public:
        NodeFactory(RHI::IRHIDevice&                  device,
                    HW::Window&                       window,
                    const HedgehogSettings::Settings& settings,
                    ResourceManager&                  resourceManager);

        std::unique_ptr<IRenderNode> Create(const NodeConfig& config) const;

    private:
        std::unique_ptr<IRenderNode> CreateRenderPassNode(const NodeConfig& config) const;
        std::unique_ptr<IRenderNode> CreateTransitionNode(const NodeConfig& config) const;

        static RHI::ImageLayout ParseImageLayout(const std::string& s);

        RHI::IRHIDevice&                  m_Device;
        HW::Window&                       m_Window;
        const HedgehogSettings::Settings& m_Settings;
        ResourceManager&                  m_ResourceManager;
    };

} // namespace Renderer
