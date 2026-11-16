#include "doctest/doctest/doctest.h"

#include "Tools/ParticleEditor/ParticleEditing.hpp"

#include "HedgehogEngine/api/Particles/ParticleEffect.hpp"

#include <string>
#include <vector>

using namespace Editor;
using HedgehogEngine::ParticleEffect;
using HedgehogEngine::ParticleSubEmitter;

namespace
{
    // Sparks (0) spawning Flash (1) per death, Flash spawning Smoke (2), and Smoke alone.
    ParticleEffect MakeChain()
    {
        ParticleEffect effect = HedgehogEngine::MakeDefaultParticleEffect();
        effect.Emitters[0].Name = "Sparks";
        CHECK(AddEmitter(effect) == 1);
        CHECK(AddEmitter(effect) == 2);
        CHECK(RenameEmitter(effect, 1, "Flash"));
        CHECK(RenameEmitter(effect, 2, "Smoke"));
        CHECK(AddSubEmitter(effect, 0, 1, 2));
        CHECK(AddSubEmitter(effect, 1, 2));
        CHECK(HedgehogEngine::ValidateParticleEffect(effect).empty());
        return effect;
    }

    std::vector<std::string> Names(const ParticleEffect& effect)
    {
        std::vector<std::string> names;
        for (const auto& emitter : effect.Emitters)
            names.push_back(emitter.Name);
        return names;
    }
}

TEST_CASE("Particle editing - names stay unique, and renames refuse empty and taken names")
{
    ParticleEffect effect = HedgehogEngine::MakeDefaultParticleEffect();
    CHECK(MakeUniqueEmitterName(effect, "Emitter") == "Emitter 1");
    CHECK(MakeUniqueEmitterName(effect, "") == "Emitter 1");
    CHECK(MakeUniqueEmitterName(effect, "Fire") == "Fire");
    AddEmitter(effect);
    CHECK(DuplicateEmitter(effect, 0) == 2);
    CHECK(Names(effect) == std::vector<std::string>{ "Emitter", "Emitter 1", "Emitter 2" });
    CHECK(DuplicateEmitter(effect, 9) == 9);
    CHECK(effect.Emitters.size() == 3);

    CHECK_FALSE(RenameEmitter(effect, 1, ""));
    CHECK_FALSE(RenameEmitter(effect, 1, "Emitter"));
    CHECK_FALSE(RenameEmitter(effect, 7, "Fire"));
    CHECK(RenameEmitter(effect, 1, "Emitter 1")); // its own name
    CHECK(RenameEmitter(effect, 1, "Fire"));
    CHECK(effect.Emitters[1].Name == "Fire");
}

TEST_CASE("Particle editing - deleting an emitter drops sub-emitters naming it and shifts later ones")
{
    ParticleEffect effect = MakeChain();
    DeleteEmitter(effect, 1); // Flash
    CHECK(Names(effect) == std::vector<std::string>{ "Sparks", "Smoke" });
    CHECK(effect.Emitters[0].SubEmitters.empty());
    CHECK(HedgehogEngine::ValidateParticleEffect(effect).empty());

    effect = MakeChain();
    DeleteEmitter(effect, 0); // Sparks: Flash's reference to Smoke follows it down
    CHECK(effect.Emitters[0].SubEmitters == std::vector<ParticleSubEmitter>{ { 1, 1 } });
    CHECK(HedgehogEngine::ValidateParticleEffect(effect).empty());

    DeleteEmitter(effect, 5); // out of range: nothing
    CHECK(effect.Emitters.size() == 2);
}

TEST_CASE("Particle editing - moving an emitter remaps every sub-emitter")
{
    ParticleEffect effect = MakeChain();
    MoveEmitter(effect, 0, 2); // Sparks last
    CHECK(Names(effect) == std::vector<std::string>{ "Flash", "Smoke", "Sparks" });
    CHECK(effect.Emitters[2].SubEmitters == std::vector<ParticleSubEmitter>{ { 0, 2 } });
    CHECK(effect.Emitters[0].SubEmitters == std::vector<ParticleSubEmitter>{ { 1, 1 } });
    CHECK(HedgehogEngine::ValidateParticleEffect(effect).empty());

    MoveEmitter(effect, 2, 0); // and back
    CHECK(effect == MakeChain());
    MoveEmitter(effect, 1, 1);
    MoveEmitter(effect, 0, 3);
    CHECK(effect == MakeChain());
}

TEST_CASE("Particle editing - a sub-emitter that would come back to its parent is refused")
{
    ParticleEffect effect = MakeChain();
    CHECK(SpawnsEmitter(effect, 0, 2));
    CHECK_FALSE(SpawnsEmitter(effect, 2, 0));
    CHECK_FALSE(AddSubEmitter(effect, 2, 0)); // Smoke -> Sparks -> Flash -> Smoke
    CHECK_FALSE(AddSubEmitter(effect, 1, 1)); // itself
    CHECK_FALSE(AddSubEmitter(effect, 0, 3)); // no such emitter
    CHECK(AddSubEmitter(effect, 0, 2, 3));    // Sparks -> Smoke too: no cycle
    CHECK(effect.Emitters[0].SubEmitters.size() == 2);
    CHECK(HedgehogEngine::ValidateParticleEffect(effect).empty());
}
