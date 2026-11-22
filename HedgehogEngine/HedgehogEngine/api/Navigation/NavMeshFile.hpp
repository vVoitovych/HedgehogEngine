#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "HedgehogNavigation/api/NavigationTypes.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The .navmesh file: a baked navmesh with the settings it was baked with. Binary, little-endian:
//   "HNAV"                    4 bytes
//   version                   uint32 (NAVMESH_FILE_VERSION)
//   the build settings        12 float32 and 1 int32, in NavMeshBuildSettings' order
//   data size                 uint32
//   Detour's data             that many bytes
namespace HedgehogEngine
{
    inline constexpr std::string_view NAVMESH_EXTENSION    = ".navmesh";
    inline constexpr uint32_t         NAVMESH_FILE_VERSION = 1;

    struct NavMeshFileReadResult
    {
        HN::NavMeshBuildSettings Settings;
        std::vector<uint8_t>     Data;  // Detour's, empty on error
        std::string              Error; // what is wrong with the file
    };

    [[nodiscard]] HEDGEHOG_ENGINE_API std::vector<uint8_t> WriteNavMeshFile(const HN::NavMeshBuildSettings& settings,
                                                                            std::span<const uint8_t> data);

    // Reads what WriteNavMeshFile writes, refusing another magic, another version, a file cut short
    // and bytes past the data, each with an Error naming it.
    [[nodiscard]] HEDGEHOG_ENGINE_API NavMeshFileReadResult ReadNavMeshFile(std::span<const uint8_t> bytes);
}
