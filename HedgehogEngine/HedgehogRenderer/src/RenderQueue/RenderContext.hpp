#pragma once

#include <cstdint>
#include <functional>

namespace HedgehogEngine
{
    struct FrameData;
}

namespace RHI
{
    class IRHIDevice;
    class IRHICommandList;
}

namespace Renderer
{
    class ResourceManager;

    struct RenderContext
    {
        std::reference_wrapper<const HedgehogEngine::FrameData> m_FrameData;
        std::reference_wrapper<RHI::IRHIDevice>                 m_Device;
        std::reference_wrapper<RHI::IRHICommandList>            m_Cmd;
        std::reference_wrapper<const ResourceManager>           m_ResourceManager;
        uint32_t                                                m_FrameIndex = 0;
    };

} // namespace Renderer
