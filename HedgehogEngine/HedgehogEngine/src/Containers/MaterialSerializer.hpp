#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <string>

namespace HedgehogEngine
{
    struct MaterialData;

    class MaterialSerializer
    {
    public:
        HEDGEHOG_ENGINE_API static void Serialize(const MaterialData& material, const std::string& materialPath);
        HEDGEHOG_ENGINE_API static void Deserialize(MaterialData& material,
                                                     const std::string& virtualPath,
                                                     const FS::FileSystemManager& fileSystem);
    };
}
