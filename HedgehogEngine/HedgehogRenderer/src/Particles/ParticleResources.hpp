#pragma once

#include "ParticleSlotAllocator.hpp"

#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"
#include "HedgehogRenderer/Graph/ParticleUniform.hpp"

#include <array>
#include <cstdint>
#include <functional>
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
    inline constexpr uint32_t PARTICLE_SORT_STRIDE  = 8; // a (key, particle) pair
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

        // Finds a texture's set by its path (the resource registry's UI texture sets), or null.
        using TextureLookup = std::function<const RHI::IRHIDescriptorSet*(const std::string& path)>;

        // This frame's particles, after Sync with the same scene: per emitter instance with a slot its
        // uniform, its reset flag and its texture's set (findTexture of its emitter's texture, null
        // for none), the alive lists the other way round from last frame, and each buffer's state as
        // the frame before left it (SetFinalStates). nullptr when no instance has a slot. Valid until
        // the next BuildFrame.
        [[nodiscard]] const ParticleFrameData* BuildFrame(const HX::RenderScene& scene, const TextureLookup& findTexture);
        // The states this frame left the buffers in (the graph's last barrier on each, as
        // RenderGraphRuntime::GetLastFinalState gives them, in ParticleBuffer order), so the next
        // frame imports them from there; a buffer the frame did not touch keeps its state.
        void SetFinalStates(const std::array<std::optional<RHI::ResourceState>, PARTICLE_BUFFER_COUNT>& states);

        // Every emitter texture of the effects taken so far, for the resource registry to load.
        [[nodiscard]] const std::vector<std::string>& GetTexturePaths() const { return m_TexturePaths; }

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
        // Where the shadow view copies its depth this frame for the next to collide with
        // (GraphFrameData::ParticleCollision); BuildFrame takes it, so it lasts one frame.
        [[nodiscard]] ParticleCollisionDepth* GetCollisionDepth() { return &m_CollisionDepth; }

    private:
        struct EffectSettings
        {
            uint32_t                                         Version = 0;
            std::vector<HedgehogEngine::ParticleEmitterView> Emitters;
            std::vector<std::string>                         Textures; // one per emitter
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

        std::vector<ParticleEmitterUniform>        m_Uniforms;
        std::vector<uint8_t>                       m_Reset;
        std::vector<const RHI::IRHIDescriptorSet*> m_Textures;
        std::vector<std::string>                   m_TexturePaths;
        ParticleFrameData                          m_Frame;
        uint32_t                                   m_Parity = 0; // which alive list this frame reads

        // Each physical buffer's state as the frames before left it: the state buffer, the dead list,
        // the two alive lists, the slot rows.
        std::array<RHI::ResourceState, PARTICLE_BUFFER_COUNT> m_States{};
        // The physical buffer behind a ParticleBuffer in the last frame built.
        [[nodiscard]] size_t PhysicalIndex(ParticleBuffer buffer) const;

        uint32_t                         m_Capacity = 0;
        std::unique_ptr<RHI::IRHIBuffer> m_State;
        std::unique_ptr<RHI::IRHIBuffer> m_DeadList;
        std::unique_ptr<RHI::IRHIBuffer> m_AliveLists[2];
        std::unique_ptr<RHI::IRHIBuffer> m_SlotData;
        std::unique_ptr<RHI::IRHIBuffer> m_SortList;
        std::unique_ptr<RHI::IRHIBuffer> m_CollisionDepthBuffer;
        ParticleCollisionDepth           m_CollisionDepth;
    };
}
