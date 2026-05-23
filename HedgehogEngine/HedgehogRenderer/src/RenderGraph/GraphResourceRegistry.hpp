#pragma once

#include "RenderGraphTypes.hpp"

#include "RHI/api/IRHIBuffer.hpp"
#include "RHI/api/IRHITexture.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace RHI
{
    class IRHIDevice;
}

namespace Renderer
{
    // Owns GPU resources (textures and buffers) declared by CreateGPUResourceNode.
    // Populated during RenderGraph::Compile() and recreated on window resize for
    // resources whose extent was declared as BACKBUFFER_EXTENT.
    class GraphResourceRegistry
    {
    public:
        GraphResourceRegistry()  = default;
        ~GraphResourceRegistry() = default;

        GraphResourceRegistry(const GraphResourceRegistry&)            = delete;
        GraphResourceRegistry& operator=(const GraphResourceRegistry&) = delete;

        // Stage a texture declaration. Actual GPU creation happens in CreateResources().
        void DeclareTexture(const GraphTextureDesc& desc);

        // Stage a buffer declaration. Actual GPU creation happens in CreateResources().
        void DeclareBuffer(const GraphBufferDesc& desc);

        // Create GPU resources from all staged declarations.
        // backbufferWidth/Height are used in place of BACKBUFFER_EXTENT.
        void CreateResources(RHI::IRHIDevice& device,
                             uint32_t backbufferWidth,
                             uint32_t backbufferHeight);

        // Destroy and recreate all textures declared with BACKBUFFER_EXTENT.
        void RecreateBackbufferSized(RHI::IRHIDevice& device,
                                     uint32_t width,
                                     uint32_t height);

        // Release all owned GPU resources and clear declarations.
        void Cleanup();

        const RHI::IRHITexture* GetTexture(const std::string& name) const;
        const RHI::IRHIBuffer*  GetBuffer(const std::string& name)  const;

        bool HasTexture(const std::string& name)  const;
        bool HasBuffer(const std::string& name)   const;
        bool HasResource(const std::string& name) const;

        // Inserts all owned textures as non-owning pointers into the target registry.
        void PopulateTextureRegistry(TextureRegistry& registry) const;

    private:

        struct TextureEntry
        {
            GraphTextureDesc                  m_Desc;
            std::unique_ptr<RHI::IRHITexture> m_Texture;
        };

        struct BufferEntry
        {
            GraphBufferDesc                  m_Desc;
            std::unique_ptr<RHI::IRHIBuffer> m_Buffer;
        };

        std::vector<GraphTextureDesc> m_PendingTextureDescs;
        std::vector<GraphBufferDesc>  m_PendingBufferDescs;

        std::unordered_map<std::string, TextureEntry> m_Textures;
        std::unordered_map<std::string, BufferEntry>  m_Buffers;
    };
}
