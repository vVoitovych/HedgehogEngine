#pragma once

#include "PassBuilderRegistry.hpp"

// The pass types the engine's shipped graph assets use (RENDERING.md section 6), registered
// under these names:
//
//   DepthPrepass   slots: depth                      writes depth
//   Shadow         slots: shadowMap                  writes shadowMap
//   Forward        slots: color, depth, shadowMap    reads depth (depth test), samples shadowMap,
//                  parameters: cullBackFaces (Flag)  writes color
//   Ui             slots: target                     writes target
//
// The declarations — slots, parameters, and every graph read and write — are final: they are what
// the shipped assets and their C++ twins are held to. Shadow is declared by the shared phase
// (SharedPhase.hpp) rather than by a view graph; Forward samples its output through an import.
// DepthPrepass, Shadow and Forward record; Ui's body, which runs the application-supplied UI
// callback, is still empty. Filling it in must not change its declaration; the equivalence tests
// would catch it if it did.
namespace Renderer
{
    void RegisterEnginePassTypes(PassBuilderRegistry& registry);
}
