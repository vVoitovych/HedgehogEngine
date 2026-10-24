#include "VulkanQueryPool.hpp"

#include "VulkanDevice.hpp"

#include <cassert>

namespace RHI
{

VulkanQueryPool::VulkanQueryPool(VulkanDevice& device, uint32_t count, double tickPeriodNs, uint32_t validBits)
    : m_Device(device)
    , m_Count(count)
    , m_TickPeriodNs(tickPeriodNs)
    , m_ValidMask(validBits >= 64 ? ~0ull : (1ull << validBits) - 1)
    , m_Scratch(static_cast<size_t>(count) * 2)
{
    VkQueryPoolCreateInfo info{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
    info.queryType  = VK_QUERY_TYPE_TIMESTAMP;
    info.queryCount = count;

    VkResult result = vkCreateQueryPool(m_Device.GetHandle(), &info, nullptr, &m_Pool);
    assert(result == VK_SUCCESS && "Failed to create VkQueryPool.");
}

VulkanQueryPool::~VulkanQueryPool()
{
    if (m_Pool != VK_NULL_HANDLE)
        vkDestroyQueryPool(m_Device.GetHandle(), m_Pool, nullptr);
}

bool VulkanQueryPool::ReadTimestamps(uint32_t first, std::span<uint64_t> out) const
{
    const auto count = static_cast<uint32_t>(out.size());
    if (count == 0 || first > m_Count || count > m_Count - first)
        return false;

    // No VK_QUERY_RESULT_WAIT_BIT: an unwritten query reports availability 0 instead of blocking.
    const VkResult result = vkGetQueryPoolResults(m_Device.GetHandle(), m_Pool, first, count,
        count * 2 * sizeof(uint64_t), m_Scratch.data(), 2 * sizeof(uint64_t),
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
    if (result != VK_SUCCESS && result != VK_NOT_READY)
        return false;
    for (uint32_t i = 0; i < count; ++i)
    {
        if (m_Scratch[i * 2 + 1] == 0)
            return false;
        out[i] = m_Scratch[i * 2] & m_ValidMask;
    }
    return true;
}

} // namespace RHI
