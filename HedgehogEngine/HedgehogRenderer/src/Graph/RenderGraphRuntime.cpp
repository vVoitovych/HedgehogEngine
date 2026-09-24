#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"

#include "Logger/api/Logger.hpp"

#include <cassert>

namespace Renderer
{
    RenderGraphRuntime::RenderGraphRuntime(RHI::IRHIDevice& device, size_t arenaCapacityBytes)
        : m_Device(device)
        , m_Arena(arenaCapacityBytes)
        , m_Pool(device)
    {
    }

    void RenderGraphRuntime::BindImportedTexture(RGTexture imported, RHI::IRHITexture* real)
    {
        assert(real && "BindImportedTexture: real must not be null.");
        m_ImportedTextures[imported.Id] = real;
    }

    void RenderGraphRuntime::BindImportedBuffer(RGBuffer imported, RHI::IRHIBuffer* real)
    {
        assert(real && "BindImportedBuffer: real must not be null.");
        m_ImportedBuffers[imported.Id] = real;
    }

    RHI::IRHITexture* RenderGraphRuntime::ResolveTexture(const GraphDescription& description, RGResourceId id)
    {
        if (auto it = m_ImportedTextures.find(id); it != m_ImportedTextures.end())
            return it->second;

        if (auto it = m_AcquiredTransients.find(id); it != m_AcquiredTransients.end())
            return it->second;

        const RGResourceRecord* resource = description.FindResource(id);
        assert(resource && !resource->IsImported && !resource->IsBuffer
               && "ResolveTexture: resource is either unknown, an unbound import, or a buffer.");

        RHI::TextureDesc desc;
        desc.Width       = resource->TextureDesc.Size.Width;
        desc.Height      = resource->TextureDesc.Size.Height;
        desc.Format      = resource->TextureDesc.Format;
        desc.Usage       = resource->TextureDesc.Usage;
        // RENDERING.md section 5.4 defers relative size-policy resolution (against a real
        // result/swapchain size) to the view integration that calls this — Absolute is the
        // only policy a transient can use until that lands.
        assert(resource->TextureDesc.Size.Kind == RGSizePolicyKind::Absolute
               && "ResolveTexture: only Absolute-sized transients are supported until view "
                  "integration provides a reference size to resolve Relative* against.");

        RHI::IRHITexture* texture = m_Pool.Acquire(desc);
        m_AcquiredTransients[id] = texture;
        return texture;
    }

    RHI::IRHIBuffer* RenderGraphRuntime::ResolveBuffer(RGResourceId id)
    {
        auto it = m_ImportedBuffers.find(id);
        assert(it != m_ImportedBuffers.end()
               && "ResolveBuffer: buffers aren't pooled — every buffer a pass touches must be "
                  "imported and bound with BindImportedBuffer before Execute().");
        return it->second;
    }

    bool RenderGraphRuntime::Execute(RHI::IRHICommandList& cmdList)
    {
        const GraphDescription& description = m_Builder.GetDescription();
        const CompileResult     result      = m_Compiler.Compile(description);

        if (!result.Success)
        {
            for (const CompileError& error : result.Errors)
                LOGERROR(error.Message);
            ResetForNextFrame();
            return false;
        }

        for (const CompiledPass& pass : result.Graph.Passes)
        {
            std::vector<RHI::TextureBarrier> textureBarriers;
            textureBarriers.reserve(pass.TextureBarriers.size());
            for (const RGTextureBarrier& barrier : pass.TextureBarriers)
            {
                RHI::TextureBarrier resolved;
                resolved.Texture = ResolveTexture(description, barrier.Id);
                resolved.Before  = barrier.Before;
                resolved.After   = barrier.After;
                resolved.Range   = barrier.Range;
                textureBarriers.push_back(resolved);
            }

            std::vector<RHI::BufferBarrier> bufferBarriers;
            bufferBarriers.reserve(pass.BufferBarriers.size());
            for (const RGBufferBarrier& barrier : pass.BufferBarriers)
            {
                RHI::BufferBarrier resolved;
                resolved.Buffer = ResolveBuffer(barrier.Id);
                resolved.Before = barrier.Before;
                resolved.After  = barrier.After;
                bufferBarriers.push_back(resolved);
            }

            if (!textureBarriers.empty() || !bufferBarriers.empty())
                cmdList.Barrier(textureBarriers, bufferBarriers);

            const PassExecutionRecord& record = m_PassExecutions[pass.OriginalPassIndex];
            record.Invoke(record.PassData, record.ExecuteState, cmdList);
        }

        ResetForNextFrame();
        return true;
    }

    void RenderGraphRuntime::ResetForNextFrame()
    {
        m_Pool.RetireFrame();
        m_PassExecutions.clear();
        m_ImportedTextures.clear();
        m_ImportedBuffers.clear();
        m_AcquiredTransients.clear();
        m_Arena.Reset();
        m_Builder.Reset();
    }
}
