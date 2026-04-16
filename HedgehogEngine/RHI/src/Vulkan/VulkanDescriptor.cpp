#include "VulkanDescriptor.hpp"

#include "VulkanBuffer.hpp"
#include "VulkanDevice.hpp"
#include "VulkanSampler.hpp"
#include "VulkanTexture.hpp"
#include "VulkanTypes.hpp"

#include <cassert>

namespace RHI
{

// ── VulkanDescriptorSetLayout ─────────────────────────────────────────────────

VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(
    VulkanDevice& device, const std::vector<DescriptorBinding>& bindings)
    : m_Device(device)
{
    std::vector<VkDescriptorSetLayoutBinding> vkBindings;
    vkBindings.reserve(bindings.size());

    for (const auto& b : bindings)
    {
        VkDescriptorSetLayoutBinding vkB{};
        vkB.binding            = b.m_Binding;
        vkB.descriptorType     = VulkanTypes::ToVkDescriptorType(b.m_Type);
        vkB.descriptorCount    = b.m_Count;
        vkB.stageFlags         = VulkanTypes::ToVkShaderStage(b.m_Stages);
        vkB.pImmutableSamplers = nullptr;
        vkBindings.push_back(vkB);
    }

    VkDescriptorSetLayoutCreateInfo createInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    createInfo.bindingCount = static_cast<uint32_t>(vkBindings.size());
    createInfo.pBindings    = vkBindings.data();

    VkResult result = vkCreateDescriptorSetLayout(
        m_Device.GetHandle(), &createInfo, nullptr, &m_Layout);
    assert(result == VK_SUCCESS && "Failed to create VkDescriptorSetLayout.");
}

VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout()
{
    if (m_Layout != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(m_Device.GetHandle(), m_Layout, nullptr);
}

// ── VulkanDescriptorPool ──────────────────────────────────────────────────────

VulkanDescriptorPool::VulkanDescriptorPool(
    VulkanDevice& device, uint32_t maxSets, const std::vector<PoolSize>& poolSizes)
    : m_Device(device)
{
    std::vector<VkDescriptorPoolSize> vkSizes;
    vkSizes.reserve(poolSizes.size());
    for (const auto& s : poolSizes)
        vkSizes.push_back({ VulkanTypes::ToVkDescriptorType(s.m_Type), s.m_Count });

    VkDescriptorPoolCreateInfo createInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    createInfo.maxSets       = maxSets;
    createInfo.poolSizeCount = static_cast<uint32_t>(vkSizes.size());
    createInfo.pPoolSizes    = vkSizes.data();

    VkResult result = vkCreateDescriptorPool(
        m_Device.GetHandle(), &createInfo, nullptr, &m_Pool);
    assert(result == VK_SUCCESS && "Failed to create VkDescriptorPool.");
}

VulkanDescriptorPool::~VulkanDescriptorPool()
{
    if (m_Pool != VK_NULL_HANDLE)
        vkDestroyDescriptorPool(m_Device.GetHandle(), m_Pool, nullptr);
}

// ── VulkanDescriptorSet ───────────────────────────────────────────────────────

VulkanDescriptorSet::VulkanDescriptorSet(
    VulkanDevice& device,
    const VulkanDescriptorPool& pool,
    const VulkanDescriptorSetLayout& layout)
    : m_Device(device)
{
    VkDescriptorSetLayout layouts[] = { layout.GetHandle() };

    VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocInfo.descriptorPool     = pool.GetHandle();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = layouts;

    VkResult result = vkAllocateDescriptorSets(m_Device.GetHandle(), &allocInfo, &m_Set);
    assert(result == VK_SUCCESS && "Failed to allocate VkDescriptorSet.");
}

void VulkanDescriptorSet::EnqueueBufferWrite(
    uint32_t binding, const IRHIBuffer& buffer,
    size_t offset, size_t size, VkDescriptorType type)
{
    const auto& vkBuf = static_cast<const VulkanBuffer&>(buffer);

    PendingWrite pw{};
    pw.m_IsBuffer                = true;
    pw.m_BufferInfo.buffer       = vkBuf.GetHandle();
    pw.m_BufferInfo.offset       = static_cast<VkDeviceSize>(offset);
    pw.m_BufferInfo.range        = size == 0 ? VK_WHOLE_SIZE : static_cast<VkDeviceSize>(size);

    pw.m_Write.sType             = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    pw.m_Write.dstSet            = m_Set;
    pw.m_Write.dstBinding        = binding;
    pw.m_Write.dstArrayElement   = 0;
    pw.m_Write.descriptorCount   = 1;
    pw.m_Write.descriptorType    = type;

    m_PendingWrites.push_back(pw);
}

void VulkanDescriptorSet::WriteUniformBuffer(
    uint32_t binding, const IRHIBuffer& buffer, size_t offset, size_t size)
{
    EnqueueBufferWrite(binding, buffer, offset, size, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

void VulkanDescriptorSet::WriteStorageBuffer(
    uint32_t binding, const IRHIBuffer& buffer, size_t offset, size_t size)
{
    EnqueueBufferWrite(binding, buffer, offset, size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
}

void VulkanDescriptorSet::WriteTexture(
    uint32_t binding, const IRHITexture& texture, const IRHISampler& sampler)
{
    const auto& vkTex = static_cast<const VulkanTexture&>(texture);
    const auto& vkSam = static_cast<const VulkanSampler&>(sampler);

    PendingWrite pw{};
    pw.m_IsBuffer                 = false;
    pw.m_ImageInfo.sampler        = vkSam.GetHandle();
    pw.m_ImageInfo.imageView      = vkTex.GetViewHandle();
    pw.m_ImageInfo.imageLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    pw.m_Write.sType              = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    pw.m_Write.dstSet             = m_Set;
    pw.m_Write.dstBinding         = binding;
    pw.m_Write.dstArrayElement    = 0;
    pw.m_Write.descriptorCount    = 1;
    pw.m_Write.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    m_PendingWrites.push_back(pw);
}

void VulkanDescriptorSet::Flush()
{
    if (m_PendingWrites.empty())
        return;

    // Fix up the info pointers now that the vector won't grow any more.
    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(m_PendingWrites.size());

    for (auto& pw : m_PendingWrites)
    {
        if (pw.m_IsBuffer)
            pw.m_Write.pBufferInfo = &pw.m_BufferInfo;
        else
            pw.m_Write.pImageInfo  = &pw.m_ImageInfo;

        writes.push_back(pw.m_Write);
    }

    vkUpdateDescriptorSets(
        m_Device.GetHandle(),
        static_cast<uint32_t>(writes.size()),
        writes.data(),
        0, nullptr);

    m_PendingWrites.clear();
}

} // namespace RHI
