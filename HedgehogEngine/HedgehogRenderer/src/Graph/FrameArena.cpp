#include "HedgehogRenderer/Graph/FrameArena.hpp"

#include <cassert>
#include <cstdint>

namespace Renderer
{
    FrameArena::FrameArena(size_t capacityBytes)
        : m_Buffer(std::make_unique<std::byte[]>(capacityBytes))
        , m_Capacity(capacityBytes)
    {
    }

    void* FrameArena::Allocate(size_t size, size_t alignment)
    {
        const uintptr_t base    = reinterpret_cast<uintptr_t>(m_Buffer.get()) + m_Offset;
        const uintptr_t aligned = (base + (alignment - 1)) & ~(alignment - 1);
        const size_t alignedOffset = aligned - reinterpret_cast<uintptr_t>(m_Buffer.get());

        assert(alignedOffset + size <= m_Capacity
               && "FrameArena exhausted — grow its capacity or shrink what's placed in it this frame.");

        m_Offset = alignedOffset + size;
        return reinterpret_cast<void*>(aligned);
    }
}
