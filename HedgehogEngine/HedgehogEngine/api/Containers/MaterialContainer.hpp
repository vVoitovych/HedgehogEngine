#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <string>
#include <vector>

namespace HedgehogEngine
{
    class RenderSystem;
    struct MaterialData;

    class MaterialContainer
    {
    public:
        // A new material's base colour, in the engine's Content folder; loaded at start.
        static constexpr const char* DEFAULT_CELL_TEXTURE = "engine://Content/Textures/Default/cells.png";

        MaterialContainer()  = default;
        ~MaterialContainer() = default;

        MaterialContainer(const MaterialContainer&)            = delete;
        MaterialContainer(MaterialContainer&&)                 = delete;
        MaterialContainer& operator=(const MaterialContainer&) = delete;
        MaterialContainer& operator=(MaterialContainer&&)      = delete;

        HEDGEHOG_ENGINE_API void Update(const RenderSystem& renderSystem, const FS::FileSystemManager& fileSystem);
        HEDGEHOG_ENGINE_API void SetMaterialDirty(size_t index);

        HEDGEHOG_ENGINE_API void ClearMaterials();
        // virtualPath: full "assets://..." path chosen by the caller.
        HEDGEHOG_ENGINE_API void CreateNewMaterial(const FS::FileSystemManager& fileSystem,
                                                    const std::string& virtualPath);
        HEDGEHOG_ENGINE_API void SaveMaterial(size_t index, const FS::FileSystemManager& fileSystem);
        // relativePath: path without "assets://" prefix, chosen by the caller.
        HEDGEHOG_ENGINE_API void LoadBaseTexture(size_t index, const std::string& relativePath);

        HEDGEHOG_ENGINE_API size_t              GetMaterialCount() const;
        HEDGEHOG_ENGINE_API MaterialData&       GetMaterialDataByIndex(size_t index);
        HEDGEHOG_ENGINE_API const MaterialData& GetMaterialDataByIndex(size_t index) const;

    private:
        std::vector<MaterialData> m_Materials;
    };
}
