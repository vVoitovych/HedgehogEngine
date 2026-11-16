#pragma once

#include "HedgehogEngine/api/Particles/ParticleEffect.hpp"

#include <cstddef>
#include <optional>

// Edits of a particle curve's and gradient's keys for the Particle Effect window's widgets, as free
// functions, so the rules are tested without ImGui: keys stay sorted by time and inside 0 to 1,
// there are at most MAX_CURVE_KEYS, and a curve with no key is the constant 1. An index out of range
// changes nothing.
namespace Editor
{
    enum class CurvePreset
    {
        Constant, // 1 all its life
        FadeIn,   // 0 to 1
        FadeOut,  // 1 to 0
        EaseInOut // 0 to 1, slow at both ends
    };

    // A key at time (clamped to 0..1) and value, in time order; its index, or nullopt when the curve is full.
    std::optional<size_t> AddCurveKey(HedgehogEngine::ParticleCurve& curve, float time, float value);
    // Moves key index to time (clamped) and value, staying in time order however far it goes; its new index.
    size_t MoveCurveKey(HedgehogEngine::ParticleCurve& curve, size_t index, float time, float value);
    void   DeleteCurveKey(HedgehogEngine::ParticleCurve& curve, size_t index);
    [[nodiscard]] HedgehogEngine::ParticleCurve MakeCurvePreset(CurvePreset preset);

    // The key nearest the point (time, value), when within radius of it, the value axis scaled by
    // valueScale (a widget's value range over its time range, so the distance is as drawn).
    [[nodiscard]] std::optional<size_t> FindCurveKey(const HedgehogEngine::ParticleCurve& curve, float time, float value,
                                                     float radius, float valueScale);

    // A stop at time (clamped) of the gradient's colour there, in time order; nullopt when full.
    std::optional<size_t> AddGradientKey(HedgehogEngine::ParticleGradient& gradient, float time);
    // Moves stop index to time (clamped), staying in time order; its new index.
    size_t MoveGradientKey(HedgehogEngine::ParticleGradient& gradient, size_t index, float time);
    void   DeleteGradientKey(HedgehogEngine::ParticleGradient& gradient, size_t index);
}
