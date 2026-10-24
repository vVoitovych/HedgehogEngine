#pragma once

#include "RHI/api/IRHIQueryPool.hpp"

#include <Volk/volk.h>

#include <vector>

namespace RHI
{

class VulkanDevice;

class VulkanQueryPool final : public IRHIQueryPool
{
public:
    // validBits: the graphics queue's timestampValidBits (1 to 64).
    VulkanQueryPool(VulkanDevice& device, uint32_t count, double tickPeriodNs, uint32_t validBits);
    ~VulkanQueryPool() override;

    VulkanQueryPool(const VulkanQueryPool&)            = delete;
    VulkanQueryPool& operator=(const VulkanQueryPool&) = delete;
    VulkanQueryPool(VulkanQueryPool&&)                 = delete;
    VulkanQueryPool& operator=(VulkanQueryPool&&)      = delete;

    uint32_t GetCount() const override { return m_Count; }
    double   GetTickPeriodNs() const override { return m_TickPeriodNs; }
    bool     ReadTimestamps(uint32_t first, std::span<uint64_t> out) const override;

    VkQueryPool GetHandle() const { return m_Pool; }

private:
    VulkanDevice&                 m_Device;
    VkQueryPool                   m_Pool         = VK_NULL_HANDLE;
    uint32_t                      m_Count        = 0;
    double                        m_TickPeriodNs = 1.0;
    uint64_t                      m_ValidMask    = ~0ull;
    mutable std::vector<uint64_t> m_Scratch; // value and availability per query
};

} // namespace RHI
