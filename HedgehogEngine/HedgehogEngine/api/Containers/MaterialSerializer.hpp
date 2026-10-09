#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <string>

namespace HedgehogEngine
{
    struct MaterialData;

    // A .material file is YAML: Type (0 Opaque, 1 Cutoff, 2 Transparent), BaseColor (the base colour
    // map), BaseColorFactor [r, g, b, a], Transparency, Metallic, Roughness, MetallicRoughnessMap,
    // NormalMap, NormalScale, OcclusionMap, OcclusionStrength, EmissiveMap and EmissiveFactor
    // [r, g, b]. Every key is optional: a missing one keeps MaterialData's default and an unreadable
    // one its default with one "[Material] <source>: <key> ..." warning, so a file written before a
    // key existed still loads.
    class MaterialSerializer
    {
    public:
        // Every key, in the order above.
        [[nodiscard]] HEDGEHOG_ENGINE_API static std::string WriteText(const MaterialData& material);
        // Reads text's keys into material; false, with one error naming source and nothing changed,
        // for text that is not a YAML map.
        HEDGEHOG_ENGINE_API static bool ReadText(MaterialData& material, const std::string& text,
                                                 const std::string& source);

        HEDGEHOG_ENGINE_API static void Serialize(const MaterialData& material,
                                                   const std::string& virtualPath,
                                                   const FS::FileSystemManager& fileSystem);
        // Sets material.path to virtualPath without "assets://", then reads the file into it.
        HEDGEHOG_ENGINE_API static void Deserialize(MaterialData& material,
                                                     const std::string& virtualPath,
                                                     const FS::FileSystemManager& fileSystem);
    };
}
