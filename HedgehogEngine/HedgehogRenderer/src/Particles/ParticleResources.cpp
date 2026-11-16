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
        settings.Textures.clear();
        // The views' texture and sub-emitters point into the catalog, which may change after this.
        for (HedgehogEngine::ParticleEmitterView& emitter : settings.Emitters)
        {
            settings.Textures.emplace_back(emitter.texture);
            if (!emitter.texture.empty() &&
                std::find(m_TexturePaths.begin(), m_TexturePaths.end(), emitter.texture) == m_TexturePaths.end())
                m_TexturePaths.emplace_back(emitter.texture);
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

    size_t ParticleResources::PhysicalIndex(ParticleBuffer buffer) const
    {
        // The state buffer, the dead list, alive list 0, alive list 1, the slot rows, the sort list.
        switch (buffer)
        {
        case ParticleBuffer::State:    return 0;
        case ParticleBuffer::DeadList: return 1;
        case ParticleBuffer::AliveIn:  return 2 + m_Parity;
        case ParticleBuffer::AliveOut: return 2 + (m_Parity ^ 1u);
        case ParticleBuffer::Slots:    return 4;
        case ParticleBuffer::Sort:     return 5;
        }
        return 0;
    }

    void ParticleResources::SetFinalStates(const std::array<std::optional<RHI::ResourceState>, PARTICLE_BUFFER_COUNT>& states)
    {
        for (size_t i = 0; i < PARTICLE_BUFFER_COUNT; ++i)
            if (states[i])
                m_States[PhysicalIndex(static_cast<ParticleBuffer>(i))] = *states[i];
    }

    const ParticleFrameData* ParticleResources::BuildFrame(const HX::RenderScene& scene, const TextureLookup& findTexture)
    {
        m_Uniforms.clear();
        m_Reset.clear();
        m_Textures.clear();
        for (size_t i = 0; i < m_FrameSlots.size() && i < scene.ParticleEmitters.size(); ++i)
        {
            const std::optional<ParticleFrameSlot>& slot = m_FrameSlots[i];
            if (!slot)
                continue;
            const HX::ParticleEmitterInstance&         instance = scene.ParticleEmitters[i];
            const HedgehogEngine::ParticleEmitterView* settings = FindEmitter(instance.EffectIndex, instance.EmitterIndex);
            ParticleEmitterUniform uniform = MakeParticleEmitterUniform(*settings, instance, slot->Range);
            uniform.SlotRow                = slot->Row;
            m_Uniforms.push_back(uniform);
            m_Reset.push_back(slot->NeedsReset ? 1 : 0);
            const std::string& texture = m_Effects[instance.EffectIndex].Textures[instance.EmitterIndex];
            m_Textures.push_back(texture.empty() || !findTexture ? nullptr : findTexture(texture));
        }
        if (m_Uniforms.empty() || !m_State)
            return nullptr;

        // Last frame's survivors are this frame's alive list.
        m_Parity ^= 1u;
        const std::array<RHI::IRHIBuffer*, PARTICLE_BUFFER_COUNT> physical = {
            m_State.get(), m_DeadList.get(), m_AliveLists[0].get(), m_AliveLists[1].get(), m_SlotData.get(), m_SortList.get()
        };
        for (size_t i = 0; i < PARTICLE_BUFFER_COUNT; ++i)
        {
            const size_t index = PhysicalIndex(static_cast<ParticleBuffer>(i));
            m_Frame.Buffers[i] = physical[index];
            m_Frame.States[i]  = m_States[index];
        }
        m_Frame.Emitters = m_Uniforms;
        m_Frame.Reset    = m_Reset;
        m_Frame.Textures = m_Textures;
        return &m_Frame;
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
        // A slot's sort region, its particles rounded up to a power of two, fits in twice its range.
        m_SortList = device.CreateBuffer(particles * 2 * PARTICLE_SORT_STRIDE, RHI::BufferUsage::StorageBuffer, RHI::MemoryUsage::GpuOnly);

        // New buffers have no state yet; the slot rows keep theirs.
        for (size_t i = 0; i < PARTICLE_BUFFER_COUNT; ++i)
            if (i != PhysicalIndex(ParticleBuffer::Slots))
                m_States[i] = RHI::ResourceState::Undefined;
        for (InstanceSlot& instance : m_Slots)
            instance.Slot.NeedsReset = true;
        for (std::optional<ParticleFrameSlot>& slot : m_FrameSlots)
            if (slot)
                slot->NeedsReset = true;
    }
}
