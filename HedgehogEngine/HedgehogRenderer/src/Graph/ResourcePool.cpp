#include "HedgehogRenderer/Graph/ResourcePool.hpp"

namespace Renderer
{
    PooledTextureKey PooledTextureKey::FromDesc(const RHI::TextureDesc& desc)
    {
        PooledTextureKey key;
        key.Format      = desc.Format;
        key.Width       = desc.Width;
        key.Height      = desc.Height;
        key.Usage       = desc.Usage;
        key.Type        = desc.Type;
        key.MipLevels   = desc.MipLevels;
        key.ArrayLayers = desc.ArrayLayers;
        return key;
    }

    size_t PooledTextureKeyHash::operator()(const PooledTextureKey& key) const
    {
        // A simple boost::hash_combine-style fold — good enough for a pool with, realistically,
        // a few dozen distinct descriptors at most.
        size_t seed = 0;
        auto combine = [&seed](size_t value)
        {
            seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        };
        combine(static_cast<size_t>(key.Format));
        combine(static_cast<size_t>(key.Width));
        combine(static_cast<size_t>(key.Height));
        combine(static_cast<size_t>(key.Usage));
        combine(static_cast<size_t>(key.Type));
        combine(static_cast<size_t>(key.MipLevels));
        combine(static_cast<size_t>(key.ArrayLayers));
        return seed;
    }

    ResourcePool::ResourcePool(RHI::IRHIDevice& device)
        : m_Device(device)
    {
    }

    RHI::IRHITexture* ResourcePool::Acquire(const RHI::TextureDesc& desc)
    {
        const PooledTextureKey key = PooledTextureKey::FromDesc(desc);
        std::vector<PooledEntry>& entries = m_Pool[key];

        for (PooledEntry& entry : entries)
        {
            if (!entry.InUse)
            {
                entry.InUse = true;
                return entry.Texture.get();
            }
        }

        PooledEntry entry;
        entry.Texture = m_Device.CreateTexture(desc);
        entry.InUse   = true;
        RHI::IRHITexture* result = entry.Texture.get();
        entries.push_back(std::move(entry));
        return result;
    }

    void ResourcePool::RetireFrame()
    {
        for (auto& [key, entries] : m_Pool)
            for (PooledEntry& entry : entries)
                entry.InUse = false;
    }

    size_t ResourcePool::LiveCount() const
    {
        size_t count = 0;
        for (const auto& [key, entries] : m_Pool)
            for (const PooledEntry& entry : entries)
                if (entry.InUse)
                    ++count;
        return count;
    }

    size_t ResourcePool::RetiredCount() const
    {
        size_t count = 0;
        for (const auto& [key, entries] : m_Pool)
            for (const PooledEntry& entry : entries)
                if (!entry.InUse)
                    ++count;
        return count;
    }
}
