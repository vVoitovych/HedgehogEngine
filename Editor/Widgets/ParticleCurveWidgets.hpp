#pragma once

#include "HedgehogEngine/api/Particles/ParticleEffect.hpp"

// The Particle Effect window's curve and gradient editors (ImGui), over the rules of
// Tools/ParticleEditor/CurveEditing. Each fills the width available and returns true on a frame
// it changed what it edits.
namespace Editor
{
    // The curve over a particle's life, time along x and its value up from 0 (to at least 1, more
    // when a key is higher): drag a key to move it, double-click to add one, right-click a key to
    // delete it or the background for the presets.
    bool ParticleCurveEditor(const char* id, HedgehogEngine::ParticleCurve& curve);

    // The gradient as a strip over a row of stops: click a stop to select it (its colour edited
    // below) and drag it along, double-click the strip to add one, right-click a stop to delete it.
    bool ParticleGradientEditor(const char* id, HedgehogEngine::ParticleGradient& gradient);
}
