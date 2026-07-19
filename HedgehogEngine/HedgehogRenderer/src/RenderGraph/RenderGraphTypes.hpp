#pragma once

#include "RHI/api/RHITypes.hpp"

#include <cstdint>

namespace RHI
{
    class IRHIDevice;
    class IRHICommandList;
    class IRHISwapchain;
    class IRHIFence;
    class IRHISemaphore;
}

namespace HedgehogEngine
{
    struct FrameData;
}

namespace HedgehogSettings
{
    class Settings;
}

namespace HR
{
    class ResourceRegistry;
}

namespace Renderer
{
    // Size classes a graph transient texture can belong to. A single Invalidate(class) call
    // recreates every texture in that class and rebuilds exactly the dependent passes.
    enum class SizeClass
    {
        SwapchainRelative,
        SceneViewRelative,
        Fixed,
    };

    struct GraphTextureDesc
    {
        SizeClass         m_SizeClass   = SizeClass::SwapchainRelative;
        RHI::Format        m_Format      = RHI::Format::Undefined;
        RHI::TextureUsage   m_Usage       = RHI::TextureUsage::None;
        // Only used when m_SizeClass == Fixed.
        uint32_t            m_FixedWidth  = 0;
        uint32_t            m_FixedHeight = 0;
    };

    using ResourceHandle = uint32_t;
    inline constexpr ResourceHandle INVALID_RESOURCE_HANDLE = static_cast<ResourceHandle>(-1);

    // Canonical names for the graph's transient textures (Design decisions, workflow/current-plan.md).
    namespace GraphResourceNames
    {
        inline constexpr const char* SCENE_COLOR  = "sceneColor";
        inline constexpr const char* SCENE_DEPTH  = "sceneDepth";
        inline constexpr const char* SHADOW_DEPTH = "shadowDepth";
        inline constexpr const char* GUI_COLOR    = "guiColor";
    }

    // Frame-varying inputs handed to every pass's Update()/Execute(). Resource lookups
    // (transient textures) go through RenderGraph::GetTexture(handle), reached via
    // CreateFramebuffers(device, graph) at compile/invalidate time — passes bind the
    // resulting textures/framebuffers once rather than looking them up every frame.
    struct RenderGraphContext
    {
        const HedgehogEngine::FrameData* m_FrameData  = nullptr;
        uint32_t                          m_FrameIndex = 0;

        RHI::IRHIDevice*      m_Device      = nullptr;
        RHI::IRHICommandList* m_CommandList = nullptr;
        RHI::IRHISwapchain*   m_Swapchain   = nullptr;
        uint32_t               m_BackBufferIndex = 0;

        RHI::IRHIFence*     m_Fence                   = nullptr;
        RHI::IRHISemaphore* m_ImageAvailableSemaphore  = nullptr;
        RHI::IRHISemaphore* m_RenderFinishedSemaphore  = nullptr;

        HR::ResourceRegistry*             m_ResourceRegistry = nullptr;
        const HedgehogSettings::Settings* m_Settings         = nullptr;
    };
}
