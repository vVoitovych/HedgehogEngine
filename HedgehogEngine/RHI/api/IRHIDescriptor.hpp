#pragma once

#include "RHITypes.hpp"

namespace RHI
{

class IRHIBuffer;
class IRHITexture;
class IRHISampler;

// ── Descriptor Set Layout ─────────────────────────────────────────────────────
// Describes which binding slots exist and their types/stages.

class IRHIDescriptorSetLayout
{
public:
    virtual ~IRHIDescriptorSetLayout() = default;

    IRHIDescriptorSetLayout(const IRHIDescriptorSetLayout&)            = delete;
    IRHIDescriptorSetLayout& operator=(const IRHIDescriptorSetLayout&) = delete;
    IRHIDescriptorSetLayout(IRHIDescriptorSetLayout&&)                 = delete;
    IRHIDescriptorSetLayout& operator=(IRHIDescriptorSetLayout&&)      = delete;

protected:
    IRHIDescriptorSetLayout() = default;
};

// ── Descriptor Pool ───────────────────────────────────────────────────────────
// Source pool from which descriptor sets are allocated.

class IRHIDescriptorPool
{
public:
    virtual ~IRHIDescriptorPool() = default;

    IRHIDescriptorPool(const IRHIDescriptorPool&)            = delete;
    IRHIDescriptorPool& operator=(const IRHIDescriptorPool&) = delete;
    IRHIDescriptorPool(IRHIDescriptorPool&&)                 = delete;
    IRHIDescriptorPool& operator=(IRHIDescriptorPool&&)      = delete;

protected:
    IRHIDescriptorPool() = default;
};

// ── Descriptor Set ────────────────────────────────────────────────────────────
// Binds concrete resources to shader binding slots.
// Call Write* to queue writes, then Flush() to commit them to the GPU.

class IRHIDescriptorSet
{
public:
    virtual ~IRHIDescriptorSet() = default;

    IRHIDescriptorSet(const IRHIDescriptorSet&)            = delete;
    IRHIDescriptorSet& operator=(const IRHIDescriptorSet&) = delete;
    IRHIDescriptorSet(IRHIDescriptorSet&&)                 = delete;
    IRHIDescriptorSet& operator=(IRHIDescriptorSet&&)      = delete;

    // Write a uniform or storage buffer to the given binding.
    // size == 0 means use the full buffer size.
    virtual void WriteUniformBuffer(uint32_t binding, const IRHIBuffer& buffer,
                                    size_t offset = 0, size_t size = 0) = 0;
    virtual void WriteStorageBuffer(uint32_t binding, const IRHIBuffer& buffer,
                                    size_t offset = 0, size_t size = 0) = 0;

    // Write a combined image sampler to the given binding.
    virtual void WriteTexture(uint32_t binding, const IRHITexture& texture,
                               const IRHISampler& sampler) = 0;

    // Commit all queued writes (calls vkUpdateDescriptorSets internally).
    virtual void Flush() = 0;

protected:
    IRHIDescriptorSet() = default;
};

} // namespace RHI
