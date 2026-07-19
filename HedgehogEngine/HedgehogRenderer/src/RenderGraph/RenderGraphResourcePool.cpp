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

    void RenderGraphResourcePool::SetSceneViewSize(uint32_t width, uint32_t height)
    {
        m_SceneViewWidth  = width;
        m_SceneViewHeight = height;
    }

    void RenderGraphResourcePool::RegisterTexture(ResourceHandle handle, const GraphTextureDesc& desc)
    {
        if (handle >= m_Entries.size())
            m_Entries.resize(handle + 1);

        m_Entries[handle].m_Desc = desc;
    }

    void RenderGraphResourcePool::SetFixedSize(ResourceHandle handle, uint32_t width, uint32_t height)
    {
        assert(handle < m_Entries.size() && "Unregistered graph texture.");
        m_Entries[handle].m_Desc.m_FixedWidth  = width;
        m_Entries[handle].m_Desc.m_FixedHeight = height;
    }

    void RenderGraphResourcePool::resolveSize(const GraphTextureDesc& desc, uint32_t& outWidth, uint32_t& outHeight) const
    {
        switch (desc.m_SizeClass)
        {
        case SizeClass::SwapchainRelative:
            outWidth  = m_SwapchainWidth;
            outHeight = m_SwapchainHeight;
            break;
        case SizeClass::SceneViewRelative:
            outWidth  = m_SceneViewWidth;
            outHeight = m_SceneViewHeight;
            break;
        case SizeClass::Fixed:
            outWidth  = desc.m_FixedWidth;
            outHeight = desc.m_FixedHeight;
            break;
        }
    }

    std::vector<ResourceHandle> RenderGraphResourcePool::Recreate(SizeClass sizeClass, RHI::IRHIDevice& device)
    {
        std::vector<ResourceHandle> changed;

        for (size_t i = 0; i < m_Entries.size(); ++i)
        {
            Entry& entry = m_Entries[i];
            if (entry.m_Desc.m_SizeClass != sizeClass)
                continue;

            uint32_t width  = 0;
            uint32_t height = 0;
            resolveSize(entry.m_Desc, width, height);
            assert(width > 0 && height > 0 && "Graph transient resolved to a zero-sized texture.");

            entry.m_Texture.reset();

            RHI::TextureDesc textureDesc;
            textureDesc.m_Width  = width;
            textureDesc.m_Height = height;
            textureDesc.m_Format = entry.m_Desc.m_Format;
            textureDesc.m_Usage  = entry.m_Desc.m_Usage;
            entry.m_Texture = device.CreateTexture(textureDesc);

            changed.push_back(static_cast<ResourceHandle>(i));
        }

        return changed;
    }

    RHI::IRHITexture& RenderGraphResourcePool::GetTexture(ResourceHandle handle) const
    {
        assert(handle < m_Entries.size() && m_Entries[handle].m_Texture && "Unregistered or not-yet-created graph texture.");
        return *m_Entries[handle].m_Texture;
    }

    void RenderGraphResourcePool::Cleanup()
    {
        m_Entries.clear();
    }
}
