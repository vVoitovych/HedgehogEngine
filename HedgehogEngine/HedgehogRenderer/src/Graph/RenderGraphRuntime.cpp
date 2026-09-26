#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>

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

    void RenderGraphRuntime::SetSizeReferences(uint32_t resultWidth, uint32_t resultHeight,
                                               uint32_t swapchainWidth, uint32_t swapchainHeight)
    {
        m_ResultWidth     = resultWidth;
        m_ResultHeight    = resultHeight;
        m_SwapchainWidth  = swapchainWidth;
        m_SwapchainHeight = swapchainHeight;
    }

    RGTextureDesc RenderGraphRuntime::ResolveSize(const RGTextureDesc& desc) const
    {
        const auto scaled = [&](uint32_t extent)
        {
            return std::max(1u, static_cast<uint32_t>(std::lround(static_cast<float>(extent) * desc.Size.Scale)));
        };

        const bool hasResult    = m_ResultWidth > 0 && m_ResultHeight > 0;
        const bool hasSwapchain = m_SwapchainWidth > 0 && m_SwapchainHeight > 0;

        RGTextureDesc resolved = desc;
        if (desc.Size.Kind == RGSizePolicyKind::RelativeToResult && hasResult)
            resolved.Size = RGSizePolicy::MakeAbsolute(scaled(m_ResultWidth), scaled(m_ResultHeight));
        else if (desc.Size.Kind == RGSizePolicyKind::RelativeToSwapchain && hasSwapchain)
            resolved.Size = RGSizePolicy::MakeAbsolute(scaled(m_SwapchainWidth), scaled(m_SwapchainHeight));
        return resolved;
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
        // CreateTexture resolved any relative policy against SetSizeReferences; one still relative
        // here was declared without a reference and has no size.
        assert(resource->TextureDesc.Size.Kind == RGSizePolicyKind::Absolute
               && "ResolveTexture: a relative-sized transient was declared before SetSizeReferences.");

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
        m_LastPassTimings.clear();
        const CompileResult     result      = m_Compiler.Compile(description);

        if (!result.Success)
        {
            for (const CompileError& error : result.Errors)
                LOGERROR(error.Message);
            m_LastExecutedPassCount = 0;
            ResetForNextFrame();
            return false;
        }

        m_LastExecutedPassCount = result.Graph.Passes.size();
        m_Executing = &description;
        for (const CompiledPass& pass : result.Graph.Passes)
        {
            const auto passStart = m_PassTimingEnabled ? std::chrono::steady_clock::now()
                                                       : std::chrono::steady_clock::time_point{};

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

            if (m_PassTimingEnabled)
            {
                const auto passEnd = std::chrono::steady_clock::now();
                m_LastPassTimings.push_back({ pass.Name,
                    std::chrono::duration<double, std::milli>(passEnd - passStart).count() });
            }
        }

        ResetForNextFrame();
        return true;
    }

    RHI::IRHITexture* RenderGraphRuntime::GetTexture(RGTexture texture)
    {
        assert(m_Executing && "RenderGraphRuntime::GetTexture: only valid from a pass's execute closure.");
        return ResolveTexture(*m_Executing, texture.Id);
    }

    void RenderGraphRuntime::ResetForNextFrame()
    {
        m_Executing    = nullptr;
        m_FrameContext = nullptr;
        SetSizeReferences(0, 0, 0, 0);
        m_Pool.RetireFrame();
        m_PassExecutions.clear();
        m_ImportedTextures.clear();
        m_ImportedBuffers.clear();
        m_AcquiredTransients.clear();
        m_Arena.Reset();
        m_Builder.Reset();
    }
}
