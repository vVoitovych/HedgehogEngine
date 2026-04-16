#pragma once

#include "../RHI/IRHISyncPrimitive.hpp"

#include "volk.h"

namespace RHI
{

class VulkanDevice;

// ── Fence ─────────────────────────────────────────────────────────────────────

class VulkanFence final : public IRHIFence
{
public:
    VulkanFence(VulkanDevice& device, bool signaled);
    ~VulkanFence() override;

    VulkanFence(const VulkanFence&)            = delete;
    VulkanFence& operator=(const VulkanFence&) = delete;
    VulkanFence(VulkanFence&&)                 = delete;
    VulkanFence& operator=(VulkanFence&&)      = delete;

    void Wait()  override;
    void Reset() override;

    VkFence GetHandle() const { return m_Fence; }

private:
    VulkanDevice& m_Device;
    VkFence       m_Fence = VK_NULL_HANDLE;
};

// ── Semaphore ─────────────────────────────────────────────────────────────────

class VulkanSemaphore final : public IRHISemaphore
{
public:
    explicit VulkanSemaphore(VulkanDevice& device);
    ~VulkanSemaphore() override;

    VulkanSemaphore(const VulkanSemaphore&)            = delete;
    VulkanSemaphore& operator=(const VulkanSemaphore&) = delete;
    VulkanSemaphore(VulkanSemaphore&&)                 = delete;
    VulkanSemaphore& operator=(VulkanSemaphore&&)      = delete;

    VkSemaphore GetHandle() const { return m_Semaphore; }

private:
    VulkanDevice& m_Device;
    VkSemaphore   m_Semaphore = VK_NULL_HANDLE;
};

} // namespace RHI
