#include "VulkanBuffer.hpp"

#include "VulkanDevice.hpp"
#include "VulkanTypes.hpp"

#include <cassert>
#include <cstring>

namespace RHI
{

VulkanBuffer::VulkanBuffer(VulkanDevice& device, size_t size, BufferUsage usage, MemoryUsage memUsage)
    : m_Device(device), m_Size(size)
{
    VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.size        = static_cast<VkDeviceSize>(size);
    bufferInfo.usage       = VulkanTypes::ToVkBufferUsage(usage);
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VulkanTypes::ToVmaMemoryUsage(memUsage);

    if (memUsage == MemoryUsage::CpuToGpu)
    {
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT
                        | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    VmaAllocationInfo allocResult{};
    VkResult result = vmaCreateBuffer(
        m_Device.GetAllocator(), &bufferInfo, &allocInfo,
        &m_Buffer, &m_Allocation, &allocResult);
    assert(result == VK_SUCCESS && "Failed to create VulkanBuffer.");

    // Keep the persistent mapped pointer for CPU-writable buffers.
    if (memUsage == MemoryUsage::CpuToGpu)
        m_MappedData = allocResult.pMappedData;
}

VulkanBuffer::~VulkanBuffer()
{
    if (m_Buffer != VK_NULL_HANDLE)
        vmaDestroyBuffer(m_Device.GetAllocator(), m_Buffer, m_Allocation);
}

void VulkanBuffer::CopyData(const void* data, size_t size, size_t offset)
{
    assert(m_MappedData && "CopyData requires a CPU-accessible buffer (CpuToGpu).");
    assert(offset + size <= m_Size && "Write out of buffer bounds.");
    memcpy(static_cast<char*>(m_MappedData) + offset, data, size);
}

void* VulkanBuffer::Map()
{
    if (!m_MappedData)
    {
        VkResult result = vmaMapMemory(m_Device.GetAllocator(), m_Allocation, &m_MappedData);
        assert(result == VK_SUCCESS && "Failed to map buffer memory.");
    }
    return m_MappedData;
}

void VulkanBuffer::Unmap()
{
    if (m_MappedData)
    {
        vmaUnmapMemory(m_Device.GetAllocator(), m_Allocation);
        m_MappedData = nullptr;
    }
}

} // namespace RHI
