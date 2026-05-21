#pragma once

#include "RHI/api/RHITypes.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace RHI
{
    class IRHITexture;
}

namespace Renderer
{
    enum class ResourceType
    {
        Color,
        Depth,
        Shadow,
    };

    enum class ResourceAccess
    {
        Read,
        Write,
        ReadWrite,
    };

    struct ResourceSlotDesc
    {
        std::string      name;
        ResourceType     type;
        ResourceAccess   access;
        // Post-write image layout for output slots; required layout for input slots.
        RHI::ImageLayout layout = RHI::ImageLayout::Undefined;
    };

    struct RenderNodeDesc
    {
        std::string                   name;
        std::vector<ResourceSlotDesc> inputs;
        std::vector<ResourceSlotDesc> outputs;
    };

    // Emitted by RenderGraph::Execute() before the consumer node runs.
    struct BarrierEntry
    {
        const RHI::IRHITexture* m_Texture    = nullptr;
        RHI::ImageLayout        m_FromLayout = RHI::ImageLayout::Undefined;
        RHI::ImageLayout        m_ToLayout   = RHI::ImageLayout::Undefined;
    };

    // Maps resource name → non-owning texture pointer. Built by RenderGraph::Compile()
    // and rebuilt after any resize that invalidates texture handles.
    using TextureRegistry = std::unordered_map<std::string, const RHI::IRHITexture*>;
}
