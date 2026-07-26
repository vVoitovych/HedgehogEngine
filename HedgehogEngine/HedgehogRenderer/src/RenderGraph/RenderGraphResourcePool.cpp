#include "RenderGraphResourcePool.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHITexture.hpp"

#include <cassert>

namespace Renderer
{
    RenderGraphResourcePool::RenderGraphResourcePool()
    {
    }

    RenderGraphResourcePool::~RenderGraphResourcePool()
    {
    }

    void RenderGraphResourcePool::SetSwapchainSize(uint32_t width, uint32_t height)
    {
        m_SwapchainWidth  = width;
        m_SwapchainHeight = height;
    }

    void RenderGraphResourcePool::SetViewSize(uint32_t width, uint32_t height)
    {
        m_ViewWidth  = width;
        m_ViewHeight = height;
    }

    uint32_t RenderGraphResourcePool::RegisterTexture(const GraphTextureDesc& desc)
    {
        const uint32_t poolIndex = static_cast<uint32_t>(m_Entries.size());
        m_Entries.emplace_back();
        m_Entries[poolIndex].Desc = desc;
        return poolIndex;
    }

    void RenderGraphResourcePool::SetFixedSize(uint32_t poolIndex, uint32_t width, uint32_t height)
    {
        assert(poolIndex < m_Entries.size() && "Unregistered graph texture.");
        m_Entries[poolIndex].Desc.FixedWidth  = width;
        m_Entries[poolIndex].Desc.FixedHeight = height;
    }

    void RenderGraphResourcePool::resolveSize(const GraphTextureDesc& desc, uint32_t& outWidth, uint32_t& outHeight) const
    {
        switch (desc.TextureSizeClass)
        {
        case SizeClass::SwapchainRelative:
            outWidth  = m_SwapchainWidth;
            outHeight = m_SwapchainHeight;
            break;
        case SizeClass::ViewRelative:
            outWidth  = m_ViewWidth;
            outHeight = m_ViewHeight;
            break;
        case SizeClass::Fixed:
            outWidth  = desc.FixedWidth;
            outHeight = desc.FixedHeight;
            break;
        }
    }

    std::vector<uint32_t> RenderGraphResourcePool::Recreate(SizeClass sizeClass, RHI::IRHIDevice& device)
    {
        std::vector<uint32_t> changed;

        for (size_t i = 0; i < m_Entries.size(); ++i)
        {
            Entry& entry = m_Entries[i];
            if (entry.Desc.TextureSizeClass != sizeClass)
                continue;

            uint32_t width  = 0;
            uint32_t height = 0;
            resolveSize(entry.Desc, width, height);
            assert(width > 0 && height > 0 && "Graph transient resolved to a zero-sized texture.");

            entry.Texture.reset();

            RHI::TextureDesc textureDesc;
            textureDesc.Width  = width;
            textureDesc.Height = height;
            textureDesc.Format = entry.Desc.Format;
            textureDesc.Usage  = entry.Desc.Usage;
            entry.Texture = device.CreateTexture(textureDesc);

            changed.push_back(static_cast<uint32_t>(i));
        }

        return changed;
    }

    RHI::IRHITexture& RenderGraphResourcePool::GetTexture(uint32_t poolIndex) const
    {
        assert(poolIndex < m_Entries.size() && m_Entries[poolIndex].Texture && "Unregistered or not-yet-created graph texture.");
        return *m_Entries[poolIndex].Texture;
    }

    void RenderGraphResourcePool::Cleanup()
    {
        m_Entries.clear();
    }
}
