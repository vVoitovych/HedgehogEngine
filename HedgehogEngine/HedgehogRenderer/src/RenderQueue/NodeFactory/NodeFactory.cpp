#include "NodeFactory.hpp"

#include "../IRenderNode.hpp"
#include "../Nodes/AcquireNode.hpp"
#include "../Nodes/ShadowmapNode.hpp"
#include "../Nodes/DepthPrepassNode.hpp"
#include "../Nodes/ForwardPassNode.hpp"
#include "../Nodes/TransitionNode.hpp"
#include "../Nodes/GuiNode.hpp"
#include "../Nodes/PresentNode.hpp"
#include "../../ResourceManager/ResourceManager.hpp"

#include "Logger/api/Logger.hpp"

#include "RHI/api/IRHITexture.hpp"

#include <cassert>
#include <filesystem>

namespace Renderer
{
    NodeFactory::NodeFactory(RHI::IRHIDevice&                  device,
                              HW::Window&                       window,
                              const HedgehogSettings::Settings& settings,
                              ResourceManager&                  resourceManager)
        : m_Device(device)
        , m_Window(window)
        , m_Settings(settings)
        , m_ResourceManager(resourceManager)
    {}

    std::unique_ptr<IRenderNode> NodeFactory::Create(const NodeConfig& config) const
    {
        if (config.m_Type == "Acquire")
            return std::make_unique<AcquireNode>();

        if (config.m_Type == "RenderPass")
            return CreateRenderPassNode(config);

        if (config.m_Type == "Transition")
            return CreateTransitionNode(config);

        if (config.m_Type == "Gui")
            return std::make_unique<GuiNode>(m_Window, m_Device, m_ResourceManager);

        if (config.m_Type == "Present")
            return std::make_unique<PresentNode>();

        LOGERROR("NodeFactory: unknown node type '", config.m_Type, "' in pass '", config.m_Name, "'");
        assert(false && "Unknown node type in .rq file");
        return nullptr;
    }

    std::unique_ptr<IRenderNode> NodeFactory::CreateRenderPassNode(const NodeConfig& config) const
    {
        const std::string filename = std::filesystem::path(config.m_Shader).filename().string();

        if (filename == "ShadowmapPass.shader")
            return std::make_unique<ShadowmapNode>(m_Device, m_Settings, m_ResourceManager);

        if (filename == "DepthPrepass.shader")
            return std::make_unique<DepthPrepassNode>(m_Device, m_ResourceManager);

        if (filename == "ForwardPass.shader")
            return std::make_unique<ForwardPassNode>(m_Device, m_ResourceManager);

        LOGERROR("NodeFactory: no concrete node registered for shader '",
                 config.m_Shader, "' in pass '", config.m_Name, "'");
        assert(false && "No concrete node registered for this shader in NodeFactory");
        return nullptr;
    }

    std::unique_ptr<IRenderNode> NodeFactory::CreateTransitionNode(const NodeConfig& config) const
    {
        TransitionNode::ResourceAccessor accessor;

        if (config.m_Resource == "SceneColorBuffer")
        {
            accessor = [](const ResourceManager& rm) -> RHI::IRHITexture&
            {
                return const_cast<RHI::IRHITexture&>(rm.GetSceneColorBuffer());
            };
        }
        else if (config.m_Resource == "RHIColorBuffer")
        {
            accessor = [](const ResourceManager& rm) -> RHI::IRHITexture&
            {
                return const_cast<RHI::IRHITexture&>(rm.GetRHIColorBuffer());
            };
        }
        else if (config.m_Resource == "RHIDepthBuffer")
        {
            accessor = [](const ResourceManager& rm) -> RHI::IRHITexture&
            {
                return const_cast<RHI::IRHITexture&>(rm.GetRHIDepthBuffer());
            };
        }
        else if (config.m_Resource == "RHIShadowMap")
        {
            accessor = [](const ResourceManager& rm) -> RHI::IRHITexture&
            {
                return const_cast<RHI::IRHITexture&>(rm.GetRHIShadowMap());
            };
        }
        else if (config.m_Resource == "RHIShadowMask")
        {
            accessor = [](const ResourceManager& rm) -> RHI::IRHITexture&
            {
                return const_cast<RHI::IRHITexture&>(rm.GetRHIShadowMask());
            };
        }
        else
        {
            LOGERROR("NodeFactory: unknown resource '", config.m_Resource,
                     "' in Transition pass '", config.m_Name, "'");
            assert(false && "Unknown resource name in .rq Transition node");
            return nullptr;
        }

        const RHI::ImageLayout fromLayout = ParseImageLayout(config.m_FromLayout);
        const RHI::ImageLayout toLayout   = ParseImageLayout(config.m_ToLayout);

        return std::make_unique<TransitionNode>(std::move(accessor), fromLayout, toLayout);
    }

    RHI::ImageLayout NodeFactory::ParseImageLayout(const std::string& s)
    {
        if (s == "Undefined")               return RHI::ImageLayout::Undefined;
        if (s == "General")                 return RHI::ImageLayout::General;
        if (s == "ColorAttachment")         return RHI::ImageLayout::ColorAttachment;
        if (s == "DepthStencilAttachment")  return RHI::ImageLayout::DepthStencilAttachment;
        if (s == "DepthStencilReadOnly")    return RHI::ImageLayout::DepthStencilReadOnly;
        if (s == "ShaderReadOnly")          return RHI::ImageLayout::ShaderReadOnly;
        if (s == "TransferSrc")             return RHI::ImageLayout::TransferSrc;
        if (s == "TransferDst")             return RHI::ImageLayout::TransferDst;
        if (s == "Present")                 return RHI::ImageLayout::Present;

        LOGERROR("NodeFactory: unknown image layout '", s, "'");
        assert(false && "Unknown image layout in .rq file");
        return RHI::ImageLayout::Undefined;
    }

} // namespace Renderer
