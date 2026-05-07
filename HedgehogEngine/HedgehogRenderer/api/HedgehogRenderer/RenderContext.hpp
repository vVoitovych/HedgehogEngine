#pragma once

#include "RHI/api/IRHIBuffer.hpp"
#include "RHI/api/IRHIDescriptor.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace HedgehogEngine { struct FrameData;       }
namespace RHI             { class IRHIDevice;
                            class IRHICommandList; }

namespace Renderer
{
    class ResourceManager;

    // Passed to IRenderNode::Render every frame.
    // Contains everything needed for command recording.
    // m_FrameIndex is private — use CurrentDescriptorSet / CurrentBuffer to bind
    // per-frame ring resources without knowing the index.
    struct RenderContext
    {
        RenderContext(const HedgehogEngine::FrameData& frame,
                      RHI::IRHIDevice&                 device,
                      RHI::IRHICommandList&             cmd,
                      const ResourceManager&           resourceManager,
                      uint32_t                         frameIndex)
            : m_FrameData(frame)
            , m_Device(device)
            , m_Cmd(cmd)
            , m_ResourceManager(resourceManager)
            , m_FrameIndex(frameIndex)
        {}

        std::reference_wrapper<const HedgehogEngine::FrameData> m_FrameData;
        std::reference_wrapper<RHI::IRHIDevice>                 m_Device;
        std::reference_wrapper<RHI::IRHICommandList>            m_Cmd;
        std::reference_wrapper<const ResourceManager>           m_ResourceManager;

        RHI::IRHIDescriptorSet& CurrentDescriptorSet(
            std::vector<std::unique_ptr<RHI::IRHIDescriptorSet>>& v) const
        {
            return *v[m_FrameIndex];
        }

        RHI::IRHIBuffer& CurrentBuffer(
            std::vector<std::unique_ptr<RHI::IRHIBuffer>>& v) const
        {
            return *v[m_FrameIndex];
        }

    private:
        uint32_t m_FrameIndex = 0;
    };

} // namespace Renderer
