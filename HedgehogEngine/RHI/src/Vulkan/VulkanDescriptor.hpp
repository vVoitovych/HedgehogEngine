#pragma once

#include "api/IRHIDescriptor.hpp"

#include <Volk/volk.h>

#include <vector>

namespace RHI
{

class VulkanDevice;

// ── Descriptor Set Layout ─────────────────────────────────────────────────────

class VulkanDescriptorSetLayout final : public IRHIDescriptorSetLayout
{
public:
    VulkanDescriptorSetLayout(VulkanDevice& device,
                               const std::vector<DescriptorBinding>& bindings);
    ~VulkanDescriptorSetLayout() override;

    VulkanDescriptorSetLayout(const VulkanDescriptorSetLayout&)            = delete;
    VulkanDescriptorSetLayout& operator=(const VulkanDescriptorSetLayout&) = delete;
    VulkanDescriptorSetLayout(VulkanDescriptorSetLayout&&)                 = delete;
    VulkanDescriptorSetLayout& operator=(VulkanDescriptorSetLayout&&)      = delete;

    VkDescriptorSetLayout GetHandle() const { return m_Layout; }

private:
    VulkanDevice&         m_Device;
    VkDescriptorSetLayout m_Layout = VK_NULL_HANDLE;
};

// ── Descriptor Pool ───────────────────────────────────────────────────────────

class VulkanDescriptorPool final : public IRHIDescriptorPool
{
public:
    VulkanDescriptorPool(VulkanDevice& device,
                          uint32_t maxSets,
                          const std::vector<PoolSize>& poolSizes);
    ~VulkanDescriptorPool() override;

    VulkanDescriptorPool(const VulkanDescriptorPool&)            = delete;
    VulkanDescriptorPool& operator=(const VulkanDescriptorPool&) = delete;
    VulkanDescriptorPool(VulkanDescriptorPool&&)                 = delete;
    VulkanDescriptorPool& operator=(VulkanDescriptorPool&&)      = delete;

    VkDescriptorPool GetHandle() const { return m_Pool; }

private:
    VulkanDevice&    m_Device;
    VkDescriptorPool m_Pool = VK_NULL_HANDLE;
};

// ── Descriptor Set ────────────────────────────────────────────────────────────

class VulkanDescriptorSet final : public IRHIDescriptorSet
{
public:
    VulkanDescriptorSet(VulkanDevice&                 device,
                         const VulkanDescriptorPool&   pool,
                         const VulkanDescriptorSetLayout& layout);
    ~VulkanDescriptorSet() override = default;

    VulkanDescriptorSet(const VulkanDescriptorSet&)            = delete;
    VulkanDescriptorSet& operator=(const VulkanDescriptorSet&) = delete;
    VulkanDescriptorSet(VulkanDescriptorSet&&)                 = delete;
    VulkanDescriptorSet& operator=(VulkanDescriptorSet&&)      = delete;

    // IRHIDescriptorSet
    void WriteUniformBuffer(uint32_t binding, const IRHIBuffer& buffer,
                             size_t offset, size_t size) override;
    void WriteStorageBuffer(uint32_t binding, const IRHIBuffer& buffer,
                             size_t offset, size_t size) override;
    void WriteTexture(uint32_t binding, const IRHITexture& texture,
                       const IRHISampler& sampler) override;
    void Flush() override;

    VkDescriptorSet GetHandle() const { return m_Set; }

private:
    // A pending write together with its associated info struct
    // (avoids dangling pointers when vectors grow).
    struct PendingWrite
    {
        VkDescriptorBufferInfo m_BufferInfo = {};
        VkDescriptorImageInfo  m_ImageInfo  = {};
        VkWriteDescriptorSet   m_Write      = {};
        bool                   m_IsBuffer   = true;
    };

    void EnqueueBufferWrite(uint32_t binding, const IRHIBuffer& buffer,
                             size_t offset, size_t size, VkDescriptorType type);

    VulkanDevice&             m_Device;
    VkDescriptorSet           m_Set = VK_NULL_HANDLE;
    std::vector<PendingWrite> m_PendingWrites;
};

} // namespace RHI
