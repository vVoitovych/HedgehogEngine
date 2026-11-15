#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"
#include "HedgehogEngine/api/Particles/ParticleEffect.hpp"

#include <optional>
#include <string>
#include <string_view>

// The text form of a ParticleEffect, a .particle file, version 1:
//
//   Version: 1
//   Emitters:
//     - Name: Fire
//       MaxParticles: 500
//       Rate: 40
//       Bursts:
//         - { Time: 0, Count: 20, Cycles: 3, Interval: 0.5 }
//       Duration: 2
//       Loop: false
//       Shape: Cone              # Point, Sphere, Cone or Box
//       Radius: 0.3
//       Angle: 15
//       BoxSize: [1, 1, 1]
//       EmitFromSurface: true
//       Lifetime: [0.5, 1]       # [Min, Max]; likewise StartSpeed, StartSize, StartRotation
//       StartColor: [1, 0.6, 0.2, 1]
//       Gravity: [0, 0, 2]
//       Drag: 0.5
//       SizeOverLife: [[0, 0.5], [1, 2]]               # [time, value] keys
//       SpeedOverLife: [[0, 1], [1, 0]]
//       ColorOverLife: [[0, 1, 1, 1, 1], [1, 1, 0, 0, 0]] # [time, r, g, b, a] keys
//       Texture: Textures/Particles/Smoke.png
//       SheetTiles: [4, 4]
//       SheetFps: 12
//       Blend: Additive          # Alpha or Additive
//       SimulationSpace: Local   # Local or World
//       SoftDistance: 0.5
//       Collision: { Mode: HeightPlane, PlaneHeight: 0, Bounce: 0.3, LifetimeLoss: 0.2 }
//       SubEmitters:
//         - { Emitter: 1, Count: 4 } # an index into Emitters
//
// Only Name is required; every other value at its default (ParticleEmitter's) is left out, as are
// a burst's Cycles (1) and Interval (1) and a collision's values at theirs. The texture is a path
// under assets://, without the prefix.
namespace HedgehogEngine
{
    inline constexpr int              PARTICLE_EFFECT_FILE_VERSION = 1;
    inline constexpr std::string_view PARTICLE_EFFECT_EXTENSION    = ".particle";

    struct ParticleEffectReadResult
    {
        std::optional<ParticleEffect> Effect; // empty on any error
        std::string                    Error;  // one message naming the emitter and key at fault
    };

    // Reads text strictly: malformed YAML, a missing or other Version, a missing Emitters or Name,
    // an unknown key or enum name, a value of the wrong kind (a number, a bool, a list of the
    // wrong length), a range with Min above Max and an effect ValidateParticleEffect refuses each
    // fail with one message.
    [[nodiscard]] HEDGEHOG_ENGINE_API ParticleEffectReadResult ParseParticleEffect(std::string_view text);

    // Writes effect in the layout above, leaving out every value at its default, so writing what
    // ParseParticleEffect read gives the same text again.
    [[nodiscard]] HEDGEHOG_ENGINE_API std::string WriteParticleEffect(const ParticleEffect& effect);
}
