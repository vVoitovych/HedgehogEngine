#pragma once

#include "api/IRHIBuffer.hpp"
#include "api/RHITypes.hpp"

#include <Volk/volk.h>
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <cstddef>

namespace RHI
{

class VulkanDevice;

class VulkanBuffer final : public IRHIBuffer
{
public:
    VulkanBuffer(VulkanDevice& device, size_t size, BufferUsage usage, MemoryUsage memUsage);
    ~VulkanBuffer() override;

    VulkanBuffer(const VulkanBuffer&)            = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;
    VulkanBuffer(VulkanBuffer&&)                 = delete;
    VulkanBuffer& operator=(VulkanBuffer&&)      = delete;

    // IRHIBuffer
    size_t GetSize() const override { return m_Size; }
    void   CopyData(const void* data, size_t size, size_t offset = 0) override;
    void*  Map()   override;
    void   Unmap() override;

    // Internal Vulkan accessor
    VkBuffer GetHandle() const { return m_Buffer; }

private:
    VulkanDevice& m_Device;

    size_t        m_Size        = 0;
    VkBuffer      m_Buffer      = VK_NULL_HANDLE;
    VmaAllocation m_Allocation  = VK_NULL_HANDLE;
    void*         m_MappedData  = nullptr;
};

} // namespace RHI
