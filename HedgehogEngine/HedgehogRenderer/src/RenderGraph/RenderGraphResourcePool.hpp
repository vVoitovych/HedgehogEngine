#pragma once

#include "RenderGraphTypes.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace RHI
{
    class IRHIDevice;
    class IRHITexture;
}

namespace Renderer
{
    // Owns every LOCALLY DECLARED transient IRHITexture a single RenderGraph instance creates,
    // keyed by a pool-local index (distinct from the graph-wide ResourceHandle space, which also
    // covers imported resources — see RenderGraph::DeclareTexture vs ImportTexture). Textures are
    // created lazily (RegisterTexture only records the desc) and (re)created in batches by size
    // class via Recreate(), which is the only place that allocates/destroys GPU textures.
    class RenderGraphResourcePool
    {
    public:
        RenderGraphResourcePool();
        ~RenderGraphResourcePool();

        RenderGraphResourcePool(const RenderGraphResourcePool&)            = delete;
        RenderGraphResourcePool& operator=(const RenderGraphResourcePool&) = delete;
        RenderGraphResourcePool(RenderGraphResourcePool&&)                 = delete;
        RenderGraphResourcePool& operator=(RenderGraphResourcePool&&)      = delete;

        void SetSwapchainSize(uint32_t width, uint32_t height);

        // Size ViewRelative transients in this pool resolve against. Meaningless (never read) for
        // a pool backing a frame- or composition-stage graph, since those never declare a
        // ViewRelative texture.
        void SetViewSize(uint32_t width, uint32_t height);

        // Registers a new transient texture description (not yet created); returns its pool-local
        // index, stable for the lifetime of this pool.
        uint32_t RegisterTexture(const GraphTextureDesc& desc);

        // Updates a Fixed-class texture's target dimensions without recreating it; a subsequent
        // Recreate(SizeClass::Fixed, ...) picks up the new size.
        void SetFixedSize(uint32_t poolIndex, uint32_t width, uint32_t height);

        // (Re)creates every registered texture whose size class matches. Destroys the previous
        // texture for each affected entry first. Returns the pool-local indices that were
        // (re)created.
        std::vector<uint32_t> Recreate(SizeClass sizeClass, RHI::IRHIDevice& device);

        RHI::IRHITexture& GetTexture(uint32_t poolIndex) const;

        void Cleanup();

    private:
        void resolveSize(const GraphTextureDesc& desc, uint32_t& outWidth, uint32_t& outHeight) const;

    private:
        struct Entry
        {
            GraphTextureDesc                  Desc;
            std::unique_ptr<RHI::IRHITexture> Texture;
        };

        std::vector<Entry> m_Entries; // indexed by pool-local index

        uint32_t m_SwapchainWidth  = 0;
        uint32_t m_SwapchainHeight = 0;
        uint32_t m_ViewWidth       = 0;
        uint32_t m_ViewHeight      = 0;
    };
}
