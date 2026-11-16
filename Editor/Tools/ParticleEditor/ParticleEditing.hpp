#pragma once

#include "HedgehogEngine/api/Particles/ParticleEffect.hpp"

#include <cstdint>
#include <string>

// Edits of a particle effect's emitter list for the Particle Effect window, as free functions, so
// they are tested without ImGui. Each keeps the sub-emitter indices pointing at the emitters they
// named, so an edit of a valid effect stays valid. An index out of range changes nothing.
namespace Editor
{
    // base, or "base 1", "base 2" ... the first no emitter is named; "Emitter" for an empty base.
    [[nodiscard]] std::string MakeUniqueEmitterName(const HedgehogEngine::ParticleEffect& effect, const std::string& base);

    // A default emitter (MakeDefaultParticleEffect's) added last, uniquely named; its index.
    uint32_t AddEmitter(HedgehogEngine::ParticleEffect& effect);
    // A copy of emitter index added last, named after it uniquely; its index, or index when out of range.
    uint32_t DuplicateEmitter(HedgehogEngine::ParticleEffect& effect, uint32_t index);
    // Removes emitter index: sub-emitters naming it go, those naming a later emitter follow it down.
    void DeleteEmitter(HedgehogEngine::ParticleEffect& effect, uint32_t index);
    // Moves emitter from to position to (the others shifting between them), sub-emitters following.
    void MoveEmitter(HedgehogEngine::ParticleEffect& effect, uint32_t from, uint32_t to);
    // Renames emitter index; false, changing nothing, for an empty name or another emitter's.
    bool RenameEmitter(HedgehogEngine::ParticleEffect& effect, uint32_t index, const std::string& name);

    // Whether emitter from spawns emitter to, through any chain of sub-emitters (or is it).
    [[nodiscard]] bool SpawnsEmitter(const HedgehogEngine::ParticleEffect& effect, uint32_t from, uint32_t to);
    // Makes child a sub-emitter of parent, count particles per death; false, changing nothing, when
    // child is out of range or already spawns parent (a cycle, which the validator refuses).
    bool AddSubEmitter(HedgehogEngine::ParticleEffect& effect, uint32_t parent, uint32_t child, uint32_t count = 1);
}
