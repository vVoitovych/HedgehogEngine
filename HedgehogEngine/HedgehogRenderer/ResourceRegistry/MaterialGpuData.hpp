#pragma once

#include <memory>

namespace RHI
{
    class IRHIBuffer;
    class IRHIDescriptorSet;
}

namespace HR
{
    struct MaterialGpuData
    {
        std::unique_ptr<RHI::IRHIBuffer>        m_UniformBuffer;
        std::unique_ptr<RHI::IRHIDescriptorSet> m_DescriptorSet;
    };
}
