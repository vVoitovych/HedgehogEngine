#include "ResourceManager.hpp"
#include "ResourceNames.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHISwapchain.hpp"
#include "RHI/api/IRHITexture.hpp"
#include "RHI/api/IRHIBuffer.hpp"

#include "Logger/api/Logger.hpp"

#include <cassert>

namespace Renderer
{
    ResourceManager::ResourceManager(const RHI::IRHISwapchain& swapchain)
        : m_Swapchain(swapchain)
    {
    }

    ResourceManager::~ResourceManager()
    {
    }

    void ResourceManager::Cleanup(RHI::IRHIDevice& device)
    {
        device.WaitIdle();
        m_Textures.clear();
        m_Buffers.clear();
    }

    void ResourceManager::SetBackBufferIndex(uint32_t index)
    {
        m_BackBufferIndex = index;
    }

    void ResourceManager::CreateTexture(std::string_view name, RHI::IRHIDevice& device,
                                        const RHI::TextureDesc& desc)
    {
        assert(name != ResourceNames::SWAP_CHAIN_BACK_BUFFER && "SWAP_CHAIN_BACK_BUFFER is reserved");
        assert(m_Textures.find(std::string(name)) == m_Textures.end() && "Texture already exists");

        m_Textures[std::string(name)] = device.CreateTexture(desc);
        LOGINFO("Texture created: {}", name);
    }

    void ResourceManager::DestroyTexture(std::string_view name)
    {
        assert(name != ResourceNames::SWAP_CHAIN_BACK_BUFFER && "Cannot destroy SWAP_CHAIN_BACK_BUFFER");
        m_Textures.erase(std::string(name));
    }

    bool ResourceManager::HasTexture(std::string_view name) const
    {
        return m_Textures.find(std::string(name)) != m_Textures.end();
    }

    const RHI::IRHITexture& ResourceManager::GetTexture(std::string_view name) const
    {
        if (name == ResourceNames::SWAP_CHAIN_BACK_BUFFER)
            return m_Swapchain.GetTexture(m_BackBufferIndex);

        auto it = m_Textures.find(std::string(name));
        assert(it != m_Textures.end() && "Texture not found");
        return *it->second;
    }

    RHI::IRHITexture& ResourceManager::GetTexture(std::string_view name)
    {
        if (name == ResourceNames::SWAP_CHAIN_BACK_BUFFER)
            return m_Swapchain.GetTexture(m_BackBufferIndex);

        auto it = m_Textures.find(std::string(name));
        assert(it != m_Textures.end() && "Texture not found");
        return *it->second;
    }

    void ResourceManager::CreateBuffer(std::string_view name, RHI::IRHIDevice& device,
                                       size_t size, RHI::BufferUsage usage, RHI::MemoryUsage memUsage)
    {
        assert(m_Buffers.find(std::string(name)) == m_Buffers.end() && "Buffer already exists");

        m_Buffers[std::string(name)] = device.CreateBuffer(size, usage, memUsage);
        LOGINFO("Buffer created: {}", name);
    }

    void ResourceManager::DestroyBuffer(std::string_view name)
    {
        m_Buffers.erase(std::string(name));
    }

    bool ResourceManager::HasBuffer(std::string_view name) const
    {
        return m_Buffers.find(std::string(name)) != m_Buffers.end();
    }

    const RHI::IRHIBuffer& ResourceManager::GetBuffer(std::string_view name) const
    {
        auto it = m_Buffers.find(std::string(name));
        assert(it != m_Buffers.end() && "Buffer not found");
        return *it->second;
    }

    RHI::IRHIBuffer& ResourceManager::GetBuffer(std::string_view name)
    {
        auto it = m_Buffers.find(std::string(name));
        assert(it != m_Buffers.end() && "Buffer not found");
        return *it->second;
    }
}
