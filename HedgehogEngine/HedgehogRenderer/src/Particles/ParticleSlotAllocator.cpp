#include "ParticleSlotAllocator.hpp"

#include <algorithm>

namespace Renderer
{
    std::optional<ParticleSlot> ParticleSlotAllocator::Allocate(uint32_t count)
    {
        if (count == 0)
            return std::nullopt;
        for (auto it = m_Free.begin(); it != m_Free.end(); ++it)
        {
            if (it->Count < count)
                continue;
            const ParticleSlot slot{ it->First, count };
            it->First += count;
            it->Count -= count;
            if (it->Count == 0)
                m_Free.erase(it);
            return slot;
        }
        if (count > m_Capacity - m_End)
            return std::nullopt;
        const ParticleSlot slot{ m_End, count };
        m_End += count;
        return slot;
    }

    void ParticleSlotAllocator::Free(ParticleSlot slot)
    {
        if (slot.Count == 0)
            return;
        auto next = std::lower_bound(m_Free.begin(), m_Free.end(), slot.First,
                                     [](const ParticleSlot& range, uint32_t first) { return range.First < first; });
        next      = m_Free.insert(next, slot);

        // Join the range after it, then the one before.
        if (auto after = next + 1; after != m_Free.end() && next->First + next->Count == after->First)
        {
            next->Count += after->Count;
            m_Free.erase(after);
        }
        if (next != m_Free.begin())
        {
            auto before = next - 1;
            if (before->First + before->Count == next->First)
            {
                before->Count += next->Count;
                next = m_Free.erase(next) - 1;
            }
        }
        // A free range at the end gives its space back to the end.
        if (next->First + next->Count == m_End)
        {
            m_End = next->First;
            m_Free.erase(next);
        }
    }
}
