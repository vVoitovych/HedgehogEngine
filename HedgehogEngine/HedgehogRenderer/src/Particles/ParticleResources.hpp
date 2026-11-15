#pragma once

#include "ParticleSlotAllocator.hpp"

#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"
#include "HedgehogRenderer/Graph/ParticleUniform.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace RHI
{
    class IRHIDevice;
    class IRHIBuffer;
}

namespace HX
{
    struct RenderScene;
}

namespace Renderer
{
    // How many particles all emitters share at most, and how many emitter instances play at once.
    inline constexpr uint32_t MAX_PARTICLES_TOTAL = 1u << 20;
    inline constexpr uint32_t MAX_PARTICLE_SLOTS  = 1024;
    // Bytes per particle in the state buffer and per slot in the slot buffer (counters and
    // indirect arguments); the compute shaders' layouts.
    inline constexpr uint32_t PARTICLE_STATE_STRIDE = 64;
    inline constexpr uint32_t PARTICLE_SLOT_STRIDE  = 64;
    // The particle buffers are made for at least this many particles, and grow by doubling.
    inline constexpr uint32_t MIN_PARTICLE_CAPACITY = 4096;

    // One emitter instance's place in the pool this frame.
    struct ParticleFrameSlot
    {
        ParticleSlot Range;
        uint32_t     Row        = 0;     // its row of the slot buffer
        bool         NeedsReset = false; // new, restarted (a new epoch), moved or its buffers remade
    };

    // The GPU side of the particles: every emitter instance of RenderScene::ParticleEmitters keeps a
    // range of the shared particle buffers (as many particles as its emitter's MaxParticles) and a
    // row of the slot buffer for as long as it is extracted, and gives them back the frame it is not.
    // The buffers are made on first use, so a scene without particles allocates nothing, and made
    // again larger when the ranges outgrow them (every instance then starts afresh).
    class ParticleResources
    {
    public:
        ParticleResources();
        ~ParticleResources();

        ParticleResources(const ParticleResources&)            = delete;
        ParticleResources& operator=(const ParticleResources&) = delete;

        // Takes the catalog's effects whose version moved on, keeping each emitter's settings.
        void SyncEffects(const HedgehogEngine::IResourceCatalog& catalog);
        // An effect's emitters, as SyncEffects keeps them (for tests, which have no catalog).
        void SetEffect(size_t index, uint32_t version, HedgehogEngine::ParticleEffectView effect);

        // Gives each of scene's emitter instances its slot, frees those no longer extracted and makes
        // the buffers fit; outside a frame's recording, as it may wait for the GPU.
        void Sync(const HX::RenderScene& scene, RHI::IRHIDevice& device);

        // This frame's work for the ParticleSimulate pass, after Sync with the same scene: a uniform
        // per emitter instance with a slot, its reset flag, and the alive lists the other way round
        // from last frame. nullptr when no instance has a slot. Valid until the next BuildFrame; the
        // pass leaves the buffers in UnorderedAccess, which the next frame starts from.
        [[nodiscard]] const ParticleFrameData* BuildFrame(const HX::RenderScene& scene);

        // The settings of an effect's emitter (its texture and sub-emitters left out), or nullptr.
        [[nodiscard]] const HedgehogEngine::ParticleEmitterView* FindEmitter(uint32_t effect, uint32_t emitter) const;
        // Per scene.ParticleEmitters entry as of the last Sync: its slot, or nullopt when it has none
        // (no settings, MaxParticles 0, or the pool or slot rows ran out, warned once).
        [[nodiscard]] std::span<const std::optional<ParticleFrameSlot>> GetFrameSlots() const { return m_FrameSlots; }

        [[nodiscard]] size_t       GetSlotCount() const { return m_Slots.size(); }
        [[nodiscard]] uint32_t     GetParticleCapacity() const { return m_Capacity; }
        [[nodiscard]] RHI::IRHIBuffer* GetStateBuffer() const { return m_State.get(); }
        [[nodiscard]] RHI::IRHIBuffer* GetDeadListBuffer() const { return m_DeadList.get(); }
        [[nodiscard]] RHI::IRHIBuffer* GetAliveListBuffer(uint32_t which) const { return m_AliveLists[which & 1].get(); }
        [[nodiscard]] RHI::IRHIBuffer* GetSlotBuffer() const { return m_SlotData.get(); }

    private:
        struct EffectSettings
        {
            uint32_t                                         Version = 0;
            std::vector<HedgehogEngine::ParticleEmitterView> Emitters;
        };

        struct InstanceSlot
        {
            uint64_t          InstanceId   = 0;
            uint32_t          EmitterIndex = 0;
            uint32_t          Epoch        = 0;
            uint32_t          Effect       = 0;
            uint32_t          Version      = 0;
            ParticleFrameSlot Slot;
            bool              Seen = false;
        };

        void Release(InstanceSlot& instance);
        void EnsureBuffers(RHI::IRHIDevice& device);

        std::vector<EffectSettings>                   m_Effects;
        std::vector<InstanceSlot>                     m_Slots;
        std::vector<uint32_t>                         m_FreeRows;
        uint32_t                                      m_NextRow = 0;
        ParticleSlotAllocator                         m_Allocator{ MAX_PARTICLES_TOTAL };
        std::vector<std::optional<ParticleFrameSlot>> m_FrameSlots;
        bool                                          m_WarnedFull = false;

        std::vector<ParticleEmitterUniform> m_Uniforms;
        std::vector<uint8_t>                m_Reset;
        ParticleFrameData                   m_Frame;
        uint32_t                            m_Parity    = 0; // which alive list this frame reads
        RHI::ResourceState                  m_LastState = RHI::ResourceState::Undefined;

        uint32_t                         m_Capacity = 0;
        std::unique_ptr<RHI::IRHIBuffer> m_State;
        std::unique_ptr<RHI::IRHIBuffer> m_DeadList;
        std::unique_ptr<RHI::IRHIBuffer> m_AliveLists[2];
        std::unique_ptr<RHI::IRHIBuffer> m_SlotData;
    };
}
