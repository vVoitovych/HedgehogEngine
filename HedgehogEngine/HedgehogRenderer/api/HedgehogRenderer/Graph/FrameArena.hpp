#pragma once

#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

// A bump allocator for one frame's worth of render-graph pass data and execute closures
// (RENDERING.md section 5.4 / the C3 ticket: "arena-allocated pass data" — "no heap churn, no
// owning pointer per pass, the graph rebuilt from scratch each frame"). Reset() rewinds the
// bump pointer without running any destructors, so every type placed here must be trivially
// destructible — Create<T> enforces that at compile time. That's a deliberate constraint, not
// an oversight: pass data and execute-closure captures are expected to be small values and
// pointers/handles into resources the arena does not own, never containers that need to
// release memory of their own.
namespace Renderer
{
    class FrameArena
    {
    public:
        explicit FrameArena(size_t capacityBytes);
        ~FrameArena() = default;

        FrameArena(const FrameArena&)            = delete;
        FrameArena& operator=(const FrameArena&) = delete;
        FrameArena(FrameArena&&)                 = delete;
        FrameArena& operator=(FrameArena&&)      = delete;

        // Placement-constructs a T in arena memory. Never call delete on the result — Reset()
        // (or the arena's own destruction) is the only cleanup, and it runs no destructor.
        template<typename T, typename... Args>
        T* Create(Args&&... args)
        {
            static_assert(std::is_trivially_destructible_v<T>,
                          "FrameArena::Create<T>: T must be trivially destructible — Reset() "
                          "never runs destructors. Store handles/values, not owning containers.");
            void* memory = Allocate(sizeof(T), alignof(T));
            return ::new (memory) T(std::forward<Args>(args)...);
        }

        // Rewinds the bump pointer to the start. Every pointer previously returned by Create()
        // is invalid after this — callers must not keep using last frame's pass data.
        void Reset() { m_Offset = 0; }

        size_t CapacityBytes() const { return m_Capacity; }
        size_t UsedBytes()     const { return m_Offset; }

        // True if ptr falls within this arena's backing storage — i.e. it came from Create(),
        // not from the heap. Exists mainly so tests can verify "no heap churn" directly instead
        // of taking it on faith.
        bool Owns(const void* ptr) const
        {
            const auto* base = m_Buffer.get();
            return ptr >= base && ptr < base + m_Capacity;
        }

    private:
        void* Allocate(size_t size, size_t alignment);

        std::unique_ptr<std::byte[]> m_Buffer;
        size_t                       m_Capacity;
        size_t                       m_Offset = 0;
    };
}
