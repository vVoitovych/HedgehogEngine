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
        std::unique_ptr<RHI::IRHIBuffer>        UniformBuffer;
        std::unique_ptr<RHI::IRHIDescriptorSet> DescriptorSet;
    };
}
