#pragma once

#include "PassBuilderRegistry.hpp"

// The pass types the engine's shipped graph assets use (RENDERING.md section 6), registered
// under these names:
//
//   DepthPrepass   slots: depth                      writes depth
//   Shadow         slots: shadowMap                  writes shadowMap
//   Forward        slots: color, depth, shadowMap    reads depth (depth test), samples shadowMap,
//                  parameters: cullBackFaces (Flag)  writes color
//   Gizmo          slots: color, depth               reads depth (depth test), writes color over
//                                                    what is there: the overlay instances' bounds
//   Ui             slots: target                     writes target; samples the view's read
//                                                    targets (GraphFrameData::UiSampledTargets)
//
// The declarations — slots, parameters, and every graph read and write — are final: they are what
// the shipped assets and their C++ twins are held to. Shadow is declared by the shared phase
// (SharedPhase.hpp) rather than by a view graph; Forward samples its output through an import.
// Every pass records. Ui runs the application's UiCallback, or clears its target without one; the
// targets it samples come from the frame context, so with none attached (the headless equivalence
// tests) its declaration is exactly the asset's.
namespace Renderer
{
    void RegisterEnginePassTypes(PassBuilderRegistry& registry);
}
