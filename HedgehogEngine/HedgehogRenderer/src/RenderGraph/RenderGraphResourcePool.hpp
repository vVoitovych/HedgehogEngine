#pragma once

#include "RenderGraphTypes.hpp"

#include <memory>
#include <vector>

namespace RHI
{
    class IRHIDevice;
    class IRHITexture;
}

namespace Renderer
{
    // Owns every transient IRHITexture the graph declares, keyed by ResourceHandle. Textures
    // are created lazily (RegisterTexture only records the desc) and (re)created in batches by
    // size class via Recreate(), which is the only place that allocates/destroys GPU textures.
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
        void SetSceneViewSize(uint32_t width, uint32_t height);

        void RegisterTexture(ResourceHandle handle, const GraphTextureDesc& desc);

        // Updates a Fixed-class texture's target dimensions without recreating it; a subsequent
        // Recreate(SizeClass::Fixed, ...) picks up the new size.
        void SetFixedSize(ResourceHandle handle, uint32_t width, uint32_t height);

        // (Re)creates every registered texture whose size class matches. Destroys the previous
        // texture for each affected handle first. Returns the handles that were (re)created.
        std::vector<ResourceHandle> Recreate(SizeClass sizeClass, RHI::IRHIDevice& device);

        RHI::IRHITexture& GetTexture(ResourceHandle handle) const;

        void Cleanup();

    private:
        void resolveSize(const GraphTextureDesc& desc, uint32_t& outWidth, uint32_t& outHeight) const;

    private:
        struct Entry
        {
            GraphTextureDesc                   m_Desc;
            std::unique_ptr<RHI::IRHITexture>  m_Texture;
        };

        std::vector<Entry> m_Entries; // indexed by ResourceHandle

        uint32_t m_SwapchainWidth  = 0;
        uint32_t m_SwapchainHeight = 0;
        uint32_t m_SceneViewWidth  = 0;
        uint32_t m_SceneViewHeight = 0;
    };
}
