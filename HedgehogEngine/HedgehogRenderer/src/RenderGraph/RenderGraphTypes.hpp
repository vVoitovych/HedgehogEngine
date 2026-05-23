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

    // ── Graph-declared resource descriptors ────────────────────────────────────

    // Sentinel width/height meaning "match the swapchain back-buffer size".
    inline constexpr uint32_t BACKBUFFER_EXTENT = 0;

    // Describes a texture resource declared by CreateGPUResourceNode.
    // TextureUsage is inferred from the format (depth → DepthStencil|Sampled,
    // others → ColorAttachment|Sampled).
    struct GraphTextureDesc
    {
        std::string  m_Name;
        RHI::Format  m_Format = RHI::Format::R8G8B8A8Unorm;
        uint32_t     m_Width  = BACKBUFFER_EXTENT;
        uint32_t     m_Height = BACKBUFFER_EXTENT;
    };

    // Describes a GPU-side storage buffer declared by CreateGPUResourceNode.
    struct GraphBufferDesc
    {
        std::string m_Name;
        size_t      m_Size = 0;
    };
}
