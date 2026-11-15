#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

// One particle effect as plain data: a list of emitters, each spawning particles at a rate and in
// bursts from a shape, with start values picked from ranges and values over each particle's life
// given by curves and a gradient. The GPU simulates and draws them; this is what it is told.
// Z-up, as the engine is; the effect sits at its entity, and in Local space moves with it.
namespace HedgehogEngine
{
    inline constexpr uint32_t MAX_PARTICLES_PER_EMITTER = 262144;
    inline constexpr uint32_t DEFAULT_MAX_PARTICLES     = 1000;
    inline constexpr size_t   MAX_CURVE_KEYS            = 8; // per curve and per gradient

    enum class ParticleShape : uint8_t
    {
        Point,  // every particle from the emitter's origin, in a random direction
        Sphere, // inside (or on) a sphere of Radius, moving outwards
        Cone,   // from a disc of Radius, within Angle degrees of +Z
        Box,    // inside (or on) a box of BoxSize, moving along +Z
    };

    enum class ParticleBlend : uint8_t
    {
        Alpha,    // over what is behind, by the particle's alpha; drawn back to front
        Additive, // added to what is behind (fire, sparks), in any order
    };

    enum class ParticleSpace : uint8_t
    {
        Local, // particles move with the entity
        World, // particles stay where they were born
    };

    enum class ParticleCollisionMode : uint8_t
    {
        None,
        HeightPlane, // a horizontal plane at PlaneHeight in the simulation space
        DepthBuffer, // the scene as the main camera last saw it
    };

    // A value picked uniformly between Min and Max for each particle.
    struct FloatRange
    {
        float Min = 0.0f;
        float Max = 0.0f;

        bool operator==(const FloatRange&) const = default;
    };

    struct ParticleCurveKey
    {
        float Time  = 0.0f; // the particle's age over its lifetime, 0 to 1
        float Value = 1.0f;

        bool operator==(const ParticleCurveKey&) const = default;
    };

    // A value over a particle's life, linear between keys sorted by time and held flat before the
    // first and after the last; with no key it is 1 (no change). At most MAX_CURVE_KEYS keys.
    struct ParticleCurve
    {
        std::vector<ParticleCurveKey> Keys;

        bool operator==(const ParticleCurve&) const = default;
    };

    struct ParticleGradientKey
    {
        float       Time  = 0.0f;
        HM::Vector4 Color = { 1.0f, 1.0f, 1.0f, 1.0f }; // linear RGBA

        bool operator==(const ParticleGradientKey&) const = default;
    };

    // A colour over a particle's life, multiplying its start colour, as a curve is evaluated; with
    // no key it is white.
    struct ParticleGradient
    {
        std::vector<ParticleGradientKey> Keys;

        bool operator==(const ParticleGradient&) const = default;
    };

    // Count particles at Time seconds into the emitter's run, then Cycles - 1 more times Interval
    // seconds apart (Cycles 0 repeats for as long as the emitter runs).
    struct ParticleBurst
    {
        float    Time     = 0.0f;
        uint32_t Count    = 10;
        uint32_t Cycles   = 1;
        float    Interval = 1.0f;

        bool operator==(const ParticleBurst&) const = default;
    };

    struct ParticleCollision
    {
        ParticleCollisionMode Mode         = ParticleCollisionMode::None;
        float                 PlaneHeight  = 0.0f;
        float                 Bounce       = 0.5f; // the share of the velocity kept, reflected
        float                 LifetimeLoss = 0.0f; // the share of its lifetime a particle loses per hit

        bool operator==(const ParticleCollision&) const = default;
    };

    // When a particle of this emitter dies, Count particles of another emitter of the same effect
    // are born where it died.
    struct ParticleSubEmitter
    {
        uint32_t Emitter = 0; // an index into the effect's emitters, never the emitter's own
        uint32_t Count   = 1;

        bool operator==(const ParticleSubEmitter&) const = default;
    };

    // The defaults are a visible white puff: 10 soft particles a second rising from a point.
    struct ParticleEmitter
    {
        std::string Name = "Emitter";

        uint32_t                   MaxParticles = DEFAULT_MAX_PARTICLES;
        float                      Rate         = 10.0f; // particles per second
        std::vector<ParticleBurst> Bursts;
        float                      Duration     = 5.0f;  // seconds of one run
        bool                       Loop         = true;

        ParticleShape Shape           = ParticleShape::Point;
        float         Radius          = 0.5f;            // Sphere and Cone
        float         Angle           = 25.0f;           // Cone, degrees from +Z
        HM::Vector3   BoxSize         = { 1.0f, 1.0f, 1.0f };
        bool          EmitFromSurface = false;           // Sphere and Box: from the surface only

        FloatRange  Lifetime      = { 1.5f, 2.0f };  // seconds
        FloatRange  StartSpeed    = { 0.5f, 1.0f };  // metres per second
        FloatRange  StartSize     = { 0.2f, 0.3f };  // metres
        FloatRange  StartRotation = { 0.0f, 360.0f }; // degrees about the view direction
        HM::Vector4 StartColor    = { 1.0f, 1.0f, 1.0f, 1.0f }; // linear RGBA
        HM::Vector3 Gravity       = { 0.0f, 0.0f, 0.0f };       // metres per second squared
        float       Drag          = 0.0f;                       // velocity lost per second, as a share

        ParticleCurve    SizeOverLife;
        ParticleCurve    SpeedOverLife;
        ParticleGradient ColorOverLife;

        std::string Texture;                   // under assets://, empty for a soft round dot
        uint32_t    SheetTilesX = 1;           // a flipbook of SheetTilesX x SheetTilesY frames
        uint32_t    SheetTilesY = 1;
        float       SheetFps    = 0.0f;        // frames per second; 0 spreads the frames over the life

        ParticleBlend     Blend           = ParticleBlend::Alpha;
        ParticleSpace     SimulationSpace = ParticleSpace::World;
        float             SoftDistance    = 0.2f; // metres over which a particle fades into what it touches; 0 for hard
        ParticleCollision Collision;
        std::vector<ParticleSubEmitter> SubEmitters;

        bool operator==(const ParticleEmitter&) const = default;
    };

    struct ParticleEffect
    {
        std::vector<ParticleEmitter> Emitters;

        bool operator==(const ParticleEffect&) const = default;
    };

    // One default emitter.
    [[nodiscard]] HEDGEHOG_ENGINE_API ParticleEffect MakeDefaultParticleEffect();

    // The curve at t (any number; outside the keys the nearest key's value).
    [[nodiscard]] HEDGEHOG_ENGINE_API float EvaluateCurve(const ParticleCurve& curve, float t);
    [[nodiscard]] HEDGEHOG_ENGINE_API HM::Vector4 EvaluateGradient(const ParticleGradient& gradient, float t);

    // Min + (Max - Min) * u, for u in [0, 1].
    [[nodiscard]] HEDGEHOG_ENGINE_API float SampleRange(const FloatRange& range, float u);

    // out.size() samples of the curve at evenly spaced times from 0 to 1 (both included; one
    // sample is the curve at 0), for a GPU lookup row. BakeGradient likewise.
    HEDGEHOG_ENGINE_API void BakeCurve(const ParticleCurve& curve, std::span<float> out);
    HEDGEHOG_ENGINE_API void BakeGradient(const ParticleGradient& gradient, std::span<HM::Vector4> out);

    // Every problem with the effect, each naming its emitter: no emitters, MaxParticles 0 or past
    // MAX_PARTICLES_PER_EMITTER, a negative or non-finite rate, duration, radius, angle, drag or
    // soft distance, a range with Min above Max or a negative lifetime or size, a curve or gradient
    // with more than MAX_CURVE_KEYS keys or keys out of 0..1 or out of order, a burst at a negative
    // time or with a non-positive interval, a sheet of 0 tiles, collision values out of range, and
    // a sub-emitter naming its own emitter, one out of range or a chain of them that comes back.
    // Empty when the effect is valid.
    [[nodiscard]] HEDGEHOG_ENGINE_API std::vector<std::string> ValidateParticleEffect(const ParticleEffect& effect);
}
