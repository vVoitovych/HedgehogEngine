#include "HedgehogRenderer/Graph/TimedCommandList.hpp"

#include <cassert>

namespace Renderer
{
    void TimedCommandList::Start(RHI::IRHICommandList& target, RHI::IRHIQueryPool& pool, uint32_t first, uint32_t capacity)
    {
        assert(capacity >= 2 && "TimedCommandList::Start: a capacity below 2 times nothing.");
        m_Target    = &target;
        m_Pool      = &pool;
        m_First     = first;
        m_Capacity  = capacity;
        m_Pass      = "";
        m_Truncated = false;
        m_Commands.clear();
        target.ResetQueries(pool, first, capacity);
        target.WriteTimestamp(pool, first);
    }

    void TimedCommandList::Stamp(GpuCommandKind kind, uint32_t count, uint32_t instances)
    {
        if (m_Commands.size() + 1 >= m_Capacity)
        {
            m_Truncated = true;
            return;
        }
        m_Commands.push_back({ m_Pass, kind, count, instances, 0.0 });
        m_Target->WriteTimestamp(*m_Pool, m_First + static_cast<uint32_t>(m_Commands.size()));
    }

    void TimedCommandList::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
    {
        m_Target->Draw(vertexCount, instanceCount, firstVertex, firstInstance);
        Stamp(GpuCommandKind::Draw, vertexCount, instanceCount);
    }

    void TimedCommandList::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex,
                                       int32_t vertexOffset, uint32_t firstInstance)
    {
        m_Target->DrawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
        Stamp(GpuCommandKind::DrawIndexed, indexCount, instanceCount);
    }

    void TimedCommandList::Barrier(std::span<const RHI::TextureBarrier> textureBarriers,
                                   std::span<const RHI::BufferBarrier>  bufferBarriers)
    {
        m_Target->Barrier(textureBarriers, bufferBarriers);
        Stamp(GpuCommandKind::Barrier, static_cast<uint32_t>(textureBarriers.size() + bufferBarriers.size()), 1);
    }

    void TimedCommandList::CopyBufferToBuffer(const RHI::IRHIBuffer& src, RHI::IRHIBuffer& dst, size_t srcOffset,
                                              size_t dstOffset, size_t size)
    {
        m_Target->CopyBufferToBuffer(src, dst, srcOffset, dstOffset, size);
        Stamp(GpuCommandKind::CopyBuffer, static_cast<uint32_t>(size), 1);
    }

    void TimedCommandList::CopyBufferToTexture(const RHI::IRHIBuffer& src, RHI::IRHITexture& dst)
    {
        m_Target->CopyBufferToTexture(src, dst);
        Stamp(GpuCommandKind::CopyToTexture, 1, 1);
    }

    void TimedCommandList::CopyBufferToTexture(const RHI::IRHIBuffer& src, RHI::IRHITexture& dst,
                                               const RHI::TextureRegion& region)
    {
        m_Target->CopyBufferToTexture(src, dst, region);
        Stamp(GpuCommandKind::CopyToTexture, 1, 1);
    }

    void TimedCommandList::GenerateMipmaps(RHI::IRHITexture& texture)
    {
        m_Target->GenerateMipmaps(texture);
        Stamp(GpuCommandKind::Mipmaps, 1, 1);
    }

    void TimedCommandList::CopyTextureToTexture(const RHI::IRHITexture& src, RHI::IRHITexture& dst)
    {
        m_Target->CopyTextureToTexture(src, dst);
        Stamp(GpuCommandKind::Blit, 1, 1);
    }
}
