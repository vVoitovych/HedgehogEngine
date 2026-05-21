#pragma once

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
        std::string    name;
        ResourceType   type;
        ResourceAccess access;
    };

    struct RenderNodeDesc
    {
        std::string                   name;
        std::vector<ResourceSlotDesc> slots;
    };

    // Maps resource name → non-owning texture pointer. Built by RenderGraph::Compile()
    // and rebuilt after any resize that invalidates texture handles.
    using TextureRegistry = std::unordered_map<std::string, const RHI::IRHITexture*>;
}
