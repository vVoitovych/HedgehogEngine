#pragma once

#include <cstdint>

namespace RHI
{
    // Process-wide counters of Vulkan validation-layer messages, incremented by the
    // debug messenger callback. Zero in builds where validation layers are disabled.
    bool     AreValidationLayersEnabled();
    uint32_t GetValidationErrorCount();
    uint32_t GetValidationWarningCount();
    void     ResetValidationCounters();
}
