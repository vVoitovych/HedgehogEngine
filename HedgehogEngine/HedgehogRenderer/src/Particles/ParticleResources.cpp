#include "ParticleResources.hpp"

#include "HedgehogExtract/api/RenderScene.hpp"

#include "RHI/api/IRHIBuffer.hpp"
#include "RHI/api/IRHIDevice.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <bit>

namespace Renderer
{
    ParticleResources::ParticleResources()  = default;
    ParticleResources::~ParticleResources() = default;

    void ParticleResources::SetEffect(size_t index, uint32_t version, HedgehogEngine::ParticleEffectView effect)
    {
        if (m_Effects.size() <= index)
            m_Effects.resize(index + 1);
        EffectSettings& settings = m_Effects[index];
        settings.Version         = version;
        settings.Emitters.assign(effect.emitters.begin(), effect.emitters.end());
        // The views' texture and sub-emitters point into the catalog, which may change after this.
        for (HedgehogEngine::ParticleEmitterView& emitter : settings.Emitters)
        {
            emitter.texture     = {};
            emitter.subEmitters = {};
        }
    }

    void ParticleResources::SyncEffects(const HedgehogEngine::IResourceCatalog& catalog)
    {
        for (size_t index = 0; index < catalog.GetParticleEffectCount(); ++index)
        {
            const uint32_t version = catalog.GetParticleEffectVersion(index);
            if (index < m_Effects.size() && m_Effects[index].Version == version)
                continue;
            SetEffect(index, version, catalog.GetParticleEffect(index));
        }
    }

    const HedgehogEngine::ParticleEmitterView* ParticleResources::FindEmitter(uint32_t effect, uint32_t emitter) const
    {
        if (effect >= m_Effects.size() || emitter >= m_Effects[effect].Emitters.size())
            return nullptr;
        return &m_Effects[effect].Emitters[emitter];
    }

    void ParticleResources::Release(InstanceSlot& instance)
    {
        m_Allocator.Free(instance.Slot.Range);
        m_FreeRows.push_back(instance.Slot.Row);
    }

    void ParticleResources::Sync(const HX::RenderScene& scene, RHI::IRHIDevice& device)
    {
        for (InstanceSlot& instance : m_Slots)
            instance.Seen = false;
        m_FrameSlots.assign(scene.ParticleEmitters.size(), std::nullopt);

        for (size_t i = 0; i < scene.ParticleEmitters.size(); ++i)
        {
            const HX::ParticleEmitterInstance&         emitter  = scene.ParticleEmitters[i];
            const HedgehogEngine::ParticleEmitterView* settings = FindEmitter(emitter.EffectIndex, emitter.EmitterIndex);
            if (!settings || settings->maxParticles == 0)
                continue;
            const uint32_t version = m_Effects[emitter.EffectIndex].Version;

            // ponytail: a linear search per emitter, fine for MAX_PARTICLE_SLOTS; a map if that grows.
            auto found = std::find_if(m_Slots.begin(), m_Slots.end(), [&](const InstanceSlot& slot)
                                      { return slot.InstanceId == emitter.InstanceId && slot.EmitterIndex == emitter.EmitterIndex; });
            // Another effect, or one whose emitter now holds another number of particles, needs
            // another range.
            if (found != m_Slots.end() && (found->Effect != emitter.EffectIndex || found->Slot.Range.Count != settings->maxParticles))
            {
                Release(*found);
                *found = m_Slots.back();
                m_Slots.pop_back();
                found = m_Slots.end();
            }

            if (found != m_Slots.end())
            {
                found->Slot.NeedsReset = found->Epoch != emitter.Epoch || found->Version != version;
                found->Epoch           = emitter.Epoch;
                found->Version         = version;
                found->Seen            = true;
                m_FrameSlots[i]        = found->Slot;
                continue;
            }

            const bool                        hasRow = !m_FreeRows.empty() || m_NextRow < MAX_PARTICLE_SLOTS;
            const std::optional<ParticleSlot> range  = hasRow ? m_Allocator.Allocate(settings->maxParticles) : std::nullopt;
            if (!range)
            {
                if (!m_WarnedFull)
                    LOGWARNING("[Particles] The particle pool is full (", MAX_PARTICLES_TOTAL, "particles,", MAX_PARTICLE_SLOTS,
                               "emitters); an emitter is not drawn.");
                m_WarnedFull = true;
                continue;
            }
            InstanceSlot instance;
            instance.InstanceId   = emitter.InstanceId;
            instance.EmitterIndex = emitter.EmitterIndex;
            instance.Epoch        = emitter.Epoch;
            instance.Effect       = emitter.EffectIndex;
            instance.Version      = version;
            instance.Seen         = true;
            instance.Slot.Range   = *range;
            instance.Slot.NeedsReset = true;
            if (!m_FreeRows.empty())
            {
                instance.Slot.Row = m_FreeRows.back();
                m_FreeRows.pop_back();
            }
            else
            {
                instance.Slot.Row = m_NextRow++;
            }
            m_Slots.push_back(instance);
            m_FrameSlots[i] = instance.Slot;
        }

        // Instances no longer extracted give their range and row back.
        for (size_t i = 0; i < m_Slots.size();)
        {
            if (m_Slots[i].Seen)
            {
                ++i;
                continue;
            }
            Release(m_Slots[i]);
            m_Slots[i] = m_Slots.back();
            m_Slots.pop_back();
        }

        EnsureBuffers(device);
    }

    void ParticleResources::EnsureBuffers(RHI::IRHIDevice& device)
    {
        if (m_Slots.empty() && !m_State)
            return; // a scene without particles allocates nothing

        if (!m_SlotData)
            m_SlotData = device.CreateBuffer(size_t{ MAX_PARTICLE_SLOTS } * PARTICLE_SLOT_STRIDE,
                                             RHI::BufferUsage::StorageBuffer | RHI::BufferUsage::IndirectBuffer |
                                                 RHI::BufferUsage::TransferDst,
                                             RHI::MemoryUsage::GpuOnly);

        const uint32_t needed = m_Allocator.GetEnd();
        if (needed <= m_Capacity)
            return;

        // Grown: the GPU may still read the old buffers, and every instance starts afresh in the new.
        if (m_State)
            device.WaitIdle();
        m_Capacity = std::max(MIN_PARTICLE_CAPACITY, std::bit_ceil(needed));
        const size_t particles = m_Capacity;
        m_State = device.CreateBuffer(particles * PARTICLE_STATE_STRIDE, RHI::BufferUsage::StorageBuffer, RHI::MemoryUsage::GpuOnly);
        m_DeadList = device.CreateBuffer(particles * sizeof(uint32_t), RHI::BufferUsage::StorageBuffer, RHI::MemoryUsage::GpuOnly);
        for (std::unique_ptr<RHI::IRHIBuffer>& alive : m_AliveLists)
            alive = device.CreateBuffer(particles * sizeof(uint32_t), RHI::BufferUsage::StorageBuffer, RHI::MemoryUsage::GpuOnly);

        for (InstanceSlot& instance : m_Slots)
            instance.Slot.NeedsReset = true;
        for (std::optional<ParticleFrameSlot>& slot : m_FrameSlots)
            if (slot)
                slot->NeedsReset = true;
    }
}
