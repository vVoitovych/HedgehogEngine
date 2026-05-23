#include "GraphResourceRegistry.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/RHITypes.hpp"

#include "Logger/api/Logger.hpp"

namespace
{
    RHI::TextureUsage InferTextureUsage(RHI::Format format)
    {
        switch (format)
        {
            case RHI::Format::D32Float:
            case RHI::Format::D16Unorm:
            case RHI::Format::D24UnormS8Uint:
            case RHI::Format::D32FloatS8Uint:
                return RHI::TextureUsage::DepthStencil | RHI::TextureUsage::Sampled;
            default:
                return RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled;
        }
    }
}

namespace Renderer
{
    void GraphResourceRegistry::DeclareTexture(const GraphTextureDesc& desc)
    {
        m_PendingTextureDescs.push_back(desc);
    }

    void GraphResourceRegistry::DeclareBuffer(const GraphBufferDesc& desc)
    {
        m_PendingBufferDescs.push_back(desc);
    }

    void GraphResourceRegistry::CreateResources(RHI::IRHIDevice& device,
                                                 uint32_t backbufferWidth,
                                                 uint32_t backbufferHeight)
    {
        for (const auto& desc : m_PendingTextureDescs)
        {
            const uint32_t w = (desc.m_Width  == BACKBUFFER_EXTENT) ? backbufferWidth  : desc.m_Width;
            const uint32_t h = (desc.m_Height == BACKBUFFER_EXTENT) ? backbufferHeight : desc.m_Height;

            RHI::TextureDesc tdesc;
            tdesc.m_Width  = w;
            tdesc.m_Height = h;
            tdesc.m_Format = desc.m_Format;
            tdesc.m_Usage  = InferTextureUsage(desc.m_Format);

            TextureEntry entry;
            entry.m_Desc    = desc;
            entry.m_Texture = device.CreateTexture(tdesc);
            m_Textures.emplace(desc.m_Name, std::move(entry));
            LOGINFO("GraphResourceRegistry: created texture '", desc.m_Name, "'");
        }
        m_PendingTextureDescs.clear();

        for (const auto& desc : m_PendingBufferDescs)
        {
            BufferEntry entry;
            entry.m_Desc   = desc;
            entry.m_Buffer = device.CreateBuffer(
                desc.m_Size, RHI::BufferUsage::StorageBuffer, RHI::MemoryUsage::GpuOnly);
            m_Buffers.emplace(desc.m_Name, std::move(entry));
            LOGINFO("GraphResourceRegistry: created buffer '", desc.m_Name, "'");
        }
        m_PendingBufferDescs.clear();
    }

    void GraphResourceRegistry::RecreateBackbufferSized(RHI::IRHIDevice& device,
                                                         uint32_t width,
                                                         uint32_t height)
    {
        for (auto& [name, entry] : m_Textures)
        {
            if (entry.m_Desc.m_Width != BACKBUFFER_EXTENT && entry.m_Desc.m_Height != BACKBUFFER_EXTENT)
                continue;

            RHI::TextureDesc tdesc;
            tdesc.m_Width  = width;
            tdesc.m_Height = height;
            tdesc.m_Format = entry.m_Desc.m_Format;
            tdesc.m_Usage  = InferTextureUsage(entry.m_Desc.m_Format);
            entry.m_Texture = device.CreateTexture(tdesc);
            LOGINFO("GraphResourceRegistry: recreated backbuffer-sized texture '", name, "'");
        }
    }

    void GraphResourceRegistry::Cleanup()
    {
        m_Textures.clear();
        m_Buffers.clear();
        m_PendingTextureDescs.clear();
        m_PendingBufferDescs.clear();
    }

    const RHI::IRHITexture* GraphResourceRegistry::GetTexture(const std::string& name) const
    {
        const auto it = m_Textures.find(name);
        return (it != m_Textures.end()) ? it->second.m_Texture.get() : nullptr;
    }

    const RHI::IRHIBuffer* GraphResourceRegistry::GetBuffer(const std::string& name) const
    {
        const auto it = m_Buffers.find(name);
        return (it != m_Buffers.end()) ? it->second.m_Buffer.get() : nullptr;
    }

    bool GraphResourceRegistry::HasTexture(const std::string& name) const
    {
        return m_Textures.find(name) != m_Textures.end();
    }

    bool GraphResourceRegistry::HasBuffer(const std::string& name) const
    {
        return m_Buffers.find(name) != m_Buffers.end();
    }

    bool GraphResourceRegistry::HasResource(const std::string& name) const
    {
        return HasTexture(name) || HasBuffer(name);
    }

    void GraphResourceRegistry::PopulateTextureRegistry(TextureRegistry& registry) const
    {
        for (const auto& [name, entry] : m_Textures)
            registry[name] = entry.m_Texture.get();
    }

}
