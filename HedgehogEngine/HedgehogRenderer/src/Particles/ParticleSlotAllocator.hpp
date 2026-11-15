#pragma once

#include "HedgehogRenderer/Graph/ParticleUniform.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace Renderer
{
    // Hands out ranges of a capacity's worth of particles: the first free range big enough, else
    // the end of what is in use. A freed range joins its free neighbours. Device-free.
    class ParticleSlotAllocator
    {
    public:
        explicit ParticleSlotAllocator(uint32_t capacity) : m_Capacity(capacity) {}

        // A range of count particles, or nullopt when count is 0 or no range that big is left.
        [[nodiscard]] std::optional<ParticleSlot> Allocate(uint32_t count);
        void Free(ParticleSlot slot);

        // One past the last particle in use: how big the buffers must be.
        [[nodiscard]] uint32_t GetEnd() const { return m_End; }
        [[nodiscard]] uint32_t GetCapacity() const { return m_Capacity; }

    private:
        uint32_t                  m_Capacity = 0;
        uint32_t                  m_End      = 0;
        std::vector<ParticleSlot> m_Free; // below m_End, by First, none touching another
    };
}
