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
// the shipped assets and their C++ twins are held to. The execute bodies are empty until the
// recording lands: DepthPrepass and Shadow in HE-80, Forward in HE-81, and Ui, which runs the
// application-supplied UI callback, in HE-85. Filling them in must not change a declaration; the
// equivalence tests would catch it if it did.
namespace Renderer
{
    void RegisterEnginePassTypes(PassBuilderRegistry& registry);
}
