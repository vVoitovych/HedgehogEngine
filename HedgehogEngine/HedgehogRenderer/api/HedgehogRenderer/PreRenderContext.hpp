#pragma once

#include "RHI/api/IRHIBuffer.hpp"
#include "RHI/api/IRHIDescriptor.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace HedgehogEngine { struct FrameData;   }
namespace RHI             { class IRHIDevice;  }
namespace HedgehogSettings{ class Settings;    }

namespace Renderer
{
    class ResourceManager;

    // Passed to IRenderNode::PreRender every frame.
    // Carries all data needed for resource preparation and descriptor writes.
    // m_FrameIndex is private — use CurrentBuffer / CurrentDescriptorSet to access
    // per-frame ring resources without knowing the index.
    struct PreRenderContext
    {
        PreRenderContext(const HedgehogEngine::FrameData&  frame,
                         RHI::IRHIDevice&                  device,
                         const ResourceManager&            resourceManager,
                         const HedgehogSettings::Settings& settings,
                         uint32_t                          frameIndex)
            : m_FrameData(frame)
            , m_Device(device)
            , m_ResourceManager(resourceManager)
            , m_Settings(settings)
            , m_FrameIndex(frameIndex)
        {}

        const HedgehogEngine::FrameData&  m_FrameData;
        RHI::IRHIDevice&                  m_Device;
        const ResourceManager&            m_ResourceManager;
        const HedgehogSettings::Settings& m_Settings;

        RHI::IRHIBuffer& CurrentBuffer(
            std::vector<std::unique_ptr<RHI::IRHIBuffer>>& v) const
        {
            return *v[m_FrameIndex];
        }

        RHI::IRHIDescriptorSet& CurrentDescriptorSet(
            std::vector<std::unique_ptr<RHI::IRHIDescriptorSet>>& v) const
        {
            return *v[m_FrameIndex];
        }

    private:
        uint32_t m_FrameIndex;
    };

} // namespace Renderer
