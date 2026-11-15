#include "../src/Particles/ParticleResources.hpp"
#include "../src/Particles/ParticleSlotAllocator.hpp"

#include "HedgehogExtract/api/RenderScene.hpp"

#include "TestRHIDoubles.hpp"

#include "doctest/doctest/doctest.h"

#include <vector>

using namespace Renderer;

TEST_CASE("Particle slots - disjoint ranges, a freed range reused, neighbours joined, the end given back")
{
    ParticleSlotAllocator slots(1000);
    const ParticleSlot a = *slots.Allocate(100);
    const ParticleSlot b = *slots.Allocate(200);
    const ParticleSlot c = *slots.Allocate(300);
    CHECK(a == ParticleSlot{ 0, 100 });
    CHECK(b == ParticleSlot{ 100, 200 });
    CHECK(c == ParticleSlot{ 300, 300 });
    CHECK(slots.GetEnd() == 600);

    // A freed range is reused, first fit, its rest kept.
    slots.Free(b);
    CHECK(slots.Allocate(150) == ParticleSlot{ 100, 150 });
    CHECK(slots.Allocate(60) == ParticleSlot{ 600, 60 }); // 50 left in the gap: too small
    CHECK(slots.Allocate(50) == ParticleSlot{ 250, 50 });

    // Freeing neighbours joins them into one range a bigger request fits.
    ParticleSlotAllocator joined(1000);
    const ParticleSlot x = *joined.Allocate(100);
    const ParticleSlot y = *joined.Allocate(100);
    const ParticleSlot z = *joined.Allocate(100);
    CHECK(joined.Allocate(100).has_value()); // keeps the end at 400
    joined.Free(x);
    joined.Free(z);
    joined.Free(y);
    CHECK(joined.Allocate(300) == ParticleSlot{ 0, 300 });

    // Freeing the last range gives its space back to the end, with a free range before it.
    ParticleSlotAllocator end(1000);
    const ParticleSlot first  = *end.Allocate(100);
    const ParticleSlot second = *end.Allocate(100);
    end.Free(first);
    end.Free(second);
    CHECK(end.GetEnd() == 0);

    // Refused past the capacity, and for 0.
    ParticleSlotAllocator small(100);
    CHECK(small.Allocate(100).has_value());
    CHECK_FALSE(small.Allocate(1).has_value());
    CHECK_FALSE(small.Allocate(0).has_value());
}

namespace
{
    // An effect of two emitters holding the given particles.
    HedgehogEngine::ParticleEffectView MakeEffect(std::vector<HedgehogEngine::ParticleEmitterView>& storage, uint32_t first,
                                                  uint32_t second)
    {
        storage.assign(2, {});
        storage[0].maxParticles = first;
        storage[1].maxParticles = second;
        return { storage };
    }

    HX::ParticleEmitterInstance Instance(uint64_t id, uint32_t emitter, uint32_t epoch = 0)
    {
        HX::ParticleEmitterInstance instance;
        instance.InstanceId   = id;
        instance.EmitterIndex = emitter;
        instance.Epoch        = epoch;
        return instance;
    }
}

TEST_CASE("Particle resources - a scene without emitters creates no buffer")
{
    RGTest::TestDevice device;
    ParticleResources  particles;
    HX::RenderScene    scene;
    particles.Sync(scene, device);
    CHECK(particles.GetStateBuffer() == nullptr);
    CHECK(particles.GetSlotBuffer() == nullptr);
    CHECK(particles.GetParticleCapacity() == 0);
}

TEST_CASE("Particle resources - each emitter instance keeps its range while it is extracted")
{
    RGTest::TestDevice                               device;
    ParticleResources                                particles;
    std::vector<HedgehogEngine::ParticleEmitterView> storage;
    particles.SetEffect(0, 1, MakeEffect(storage, 1000, 3000));

    HX::RenderScene scene;
    scene.ParticleEmitters = { Instance(7, 0), Instance(7, 1), Instance(8, 0) };
    particles.Sync(scene, device);

    auto slots = particles.GetFrameSlots();
    REQUIRE(slots.size() == 3);
    REQUIRE(slots[0]);
    REQUIRE(slots[1]);
    REQUIRE(slots[2]);
    CHECK(slots[0]->Range == ParticleSlot{ 0, 1000 });
    CHECK(slots[1]->Range == ParticleSlot{ 1000, 3000 });
    CHECK(slots[2]->Range == ParticleSlot{ 4000, 1000 });
    CHECK(slots[0]->Row != slots[1]->Row);
    CHECK(slots[0]->NeedsReset);
    CHECK(particles.GetParticleCapacity() == 8192); // 5000 rounded up to a power of two
    REQUIRE(particles.GetStateBuffer() != nullptr);
    CHECK(particles.GetStateBuffer()->GetSize() == 8192 * PARTICLE_STATE_STRIDE);
    CHECK(particles.GetAliveListBuffer(1)->GetSize() == 8192 * sizeof(uint32_t));
    REQUIRE(particles.GetSlotBuffer() != nullptr);

    // The next frame: the same ranges, nothing to reset; a new epoch resets that one alone.
    scene.ParticleEmitters[1].Epoch = 1;
    particles.Sync(scene, device);
    slots = particles.GetFrameSlots();
    CHECK(slots[0]->Range == ParticleSlot{ 0, 1000 });
    CHECK_FALSE(slots[0]->NeedsReset);
    CHECK(slots[1]->NeedsReset);
    CHECK_FALSE(slots[2]->NeedsReset);

    // Instance 7 gone: its ranges and rows go back at the frame's end (instance 9, new that frame,
    // takes the end), and a later newcomer takes the first of them.
    scene.ParticleEmitters = { Instance(8, 0), Instance(9, 0) };
    particles.Sync(scene, device);
    slots = particles.GetFrameSlots();
    CHECK(slots[0]->Range == ParticleSlot{ 4000, 1000 });
    CHECK(slots[1]->Range == ParticleSlot{ 5000, 1000 });
    CHECK(slots[1]->NeedsReset);
    CHECK(particles.GetSlotCount() == 2);

    scene.ParticleEmitters.push_back(Instance(10, 0));
    particles.Sync(scene, device);
    CHECK(particles.GetFrameSlots()[2]->Range == ParticleSlot{ 0, 1000 });
    CHECK(particles.GetSlotCount() == 3);
}

TEST_CASE("Particle resources - a reloaded effect resets its instances, a resized emitter moves")
{
    RGTest::TestDevice                               device;
    ParticleResources                                particles;
    std::vector<HedgehogEngine::ParticleEmitterView> storage;
    particles.SetEffect(0, 1, MakeEffect(storage, 100, 100));

    HX::RenderScene scene;
    scene.ParticleEmitters = { Instance(1, 0) };
    particles.Sync(scene, device);
    particles.Sync(scene, device);
    CHECK_FALSE(particles.GetFrameSlots()[0]->NeedsReset);

    // Same size, new version: kept in place, reset.
    particles.SetEffect(0, 2, MakeEffect(storage, 100, 100));
    particles.Sync(scene, device);
    CHECK(particles.GetFrameSlots()[0]->Range == ParticleSlot{ 0, 100 });
    CHECK(particles.GetFrameSlots()[0]->NeedsReset);

    // More particles: a new range.
    particles.SetEffect(0, 3, MakeEffect(storage, 300, 100));
    particles.Sync(scene, device);
    CHECK(particles.GetFrameSlots()[0]->Range == ParticleSlot{ 0, 300 });
    CHECK(particles.GetFrameSlots()[0]->NeedsReset);
}

TEST_CASE("Particle resources - no settings, or a full pool, give no slot")
{
    RGTest::TestDevice                               device;
    ParticleResources                                particles;
    std::vector<HedgehogEngine::ParticleEmitterView> storage;
    particles.SetEffect(0, 1, MakeEffect(storage, MAX_PARTICLES_TOTAL, 0));

    HX::RenderScene scene;
    // Effect 1 and emitter 5 are unknown; emitter 1 holds no particles; the second instance does not
    // fit beside the first.
    scene.ParticleEmitters = { Instance(1, 0), Instance(2, 0), Instance(3, 1), Instance(4, 5) };
    scene.ParticleEmitters.back().EffectIndex = 1;
    particles.Sync(scene, device);
    const auto slots = particles.GetFrameSlots();
    CHECK(slots[0].has_value());
    CHECK_FALSE(slots[1].has_value());
    CHECK_FALSE(slots[2].has_value());
    CHECK_FALSE(slots[3].has_value());
    CHECK(particles.GetParticleCapacity() == MAX_PARTICLES_TOTAL);
}
