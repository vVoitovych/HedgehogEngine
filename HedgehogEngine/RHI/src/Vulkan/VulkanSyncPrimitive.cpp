#include "VulkanSyncPrimitive.hpp"

#include "VulkanDevice.hpp"

#include <cassert>

namespace RHI
{

// ── VulkanFence ───────────────────────────────────────────────────────────────

VulkanFence::VulkanFence(VulkanDevice& device, bool signaled)
    : m_Device(device)
{
    VkFenceCreateInfo createInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    if (signaled)
        createInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkResult result = vkCreateFence(m_Device.GetHandle(), &createInfo, nullptr, &m_Fence);
    assert(result == VK_SUCCESS && "Failed to create VkFence.");
}

VulkanFence::~VulkanFence()
{
    if (m_Fence != VK_NULL_HANDLE)
        vkDestroyFence(m_Device.GetHandle(), m_Fence, nullptr);
}

void VulkanFence::Wait()
{
    VkResult result = vkWaitForFences(m_Device.GetHandle(), 1, &m_Fence, VK_TRUE, UINT64_MAX);
    assert(result == VK_SUCCESS && "vkWaitForFences failed.");
}

void VulkanFence::Reset()
{
    VkResult result = vkResetFences(m_Device.GetHandle(), 1, &m_Fence);
    assert(result == VK_SUCCESS && "vkResetFences failed.");
}

// ── VulkanSemaphore ───────────────────────────────────────────────────────────

VulkanSemaphore::VulkanSemaphore(VulkanDevice& device)
    : m_Device(device)
{
    VkSemaphoreCreateInfo createInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkResult result = vkCreateSemaphore(m_Device.GetHandle(), &createInfo, nullptr, &m_Semaphore);
    assert(result == VK_SUCCESS && "Failed to create VkSemaphore.");
}

VulkanSemaphore::~VulkanSemaphore()
{
    if (m_Semaphore != VK_NULL_HANDLE)
        vkDestroySemaphore(m_Device.GetHandle(), m_Semaphore, nullptr);
}

} // namespace RHI
