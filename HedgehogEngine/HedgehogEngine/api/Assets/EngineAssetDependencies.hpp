#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include <string>

namespace EcsSerialization
{
    class AssetDependencyCollector;
}

namespace HedgehogEngine
{
    // What the engine's assets reference, for EcsSerialization's AssetDependencyCollector:
    //  - components: every AssetRef property of the reflected components (MeshPath, Material, an
    //    audio clip, a UI texture and font, a camera's graph), and ScriptComponent's script, the
    //    base ActorScript it runs on and its AssetRef properties' values;
    //  - .material: its BaseColor texture;
    //  - .graph: the shaders of its pass types (EngineRenderAssets.hpp), the shadow pass's when
    //    it imports the shadow atlas;
    //  - .shader: its pipeline layout, vertex description and SPIR-V stages (build output, so a
    //    missing .spv means the shaders were not compiled);
    //  - .gltf: its external buffers and images (not data: URIs).
    // A script's code is not read: a path a script builds at run time is not found.
    HEDGEHOG_ENGINE_API void RegisterEngineAssetDependencies(EcsSerialization::AssetDependencyCollector& collector);

    // A camera's graph reference as a virtual path: a bare name ("game") is that engine graph's
    // file, anything else a .graph path (NormalizeAssetPath).
    [[nodiscard]] HEDGEHOG_ENGINE_API std::string NormalizeGraphReference(const std::string& graphName);
}
