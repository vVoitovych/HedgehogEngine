#pragma once

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHITexture.hpp"
#include "RHI/api/RHITypes.hpp"

#include <cstddef>
#include <memory>
#include <unordered_map>
#include <vector>

// RENDERING.md section 5.4: transient graph textures are pooled by descriptor hash and recycled
// **across views and across frames** — naive per-view allocation multiplies memory by the view
// count. Deliberately texture-only: nothing in the graph pools buffers today.
//
// Persistent resources (anything sampled by a later frame, anything handed to ImGui) never go
// through this pool at all — the caller creates them once, keeps the pointer, and imports them
// into the graph (GraphBuilder::ImportTexture) instead of calling Create.
namespace Renderer
{
    // Everything that makes two texture requests interchangeable. Equality/hash follow
    // TextureDesc's fields exactly — anything that would produce a different VkImage (a
    // different format, size, usage, type, or mip/layer count) is a different key.
    struct PooledTextureKey
    {
        RHI::Format       Format  = RHI::Format::Undefined;
        uint32_t           Width   = 0;
        uint32_t           Height  = 0;
        RHI::TextureUsage    Usage   = RHI::TextureUsage::None;
        RHI::TextureType      Type    = RHI::TextureType::Texture2D;
        uint32_t                MipLevels   = 1;
        uint32_t                ArrayLayers = 1;

        static PooledTextureKey FromDesc(const RHI::TextureDesc& desc);

        bool operator==(const PooledTextureKey&) const = default;
    };

    struct PooledTextureKeyHash
    {
        size_t operator()(const PooledTextureKey& key) const;
    };

    class ResourcePool
    {
    public:
        explicit ResourcePool(RHI::IRHIDevice& device);
        ~ResourcePool() = default;

        ResourcePool(const ResourcePool&)            = delete;
        ResourcePool& operator=(const ResourcePool&) = delete;
        ResourcePool(ResourcePool&&)                 = delete;
        ResourcePool& operator=(ResourcePool&&)      = delete;

        // Returns a texture matching desc: a retired one already sitting in the pool if its key
        // matches, otherwise a freshly created one. The returned pointer is owned by the pool —
        // never delete it — and stays valid until at least the next RetireFrame() that doesn't
        // re-Acquire it.
        RHI::IRHITexture* Acquire(const RHI::TextureDesc& desc);

        // Marks every texture Acquire()'d since the last RetireFrame() as available for reuse,
        // without destroying any of them — this is the "across frames" half of pooling.
        void RetireFrame();

        size_t LiveCount()    const; // currently acquired (in use this frame)
        size_t RetiredCount() const; // available for reuse

    private:
        struct PooledEntry
        {
            std::unique_ptr<RHI::IRHITexture> Texture;
            bool                                InUse = false;
        };

        RHI::IRHIDevice&                                                              m_Device;
        std::unordered_map<PooledTextureKey, std::vector<PooledEntry>, PooledTextureKeyHash> m_Pool;
    };
}
