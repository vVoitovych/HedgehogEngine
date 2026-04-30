#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include <string>
#include <vector>

namespace HedgehogEngine
{
    class RenderSystem;
    struct MaterialData;

    class MaterialContainer
    {
    public:
        MaterialContainer()  = default;
        ~MaterialContainer() = default;

        MaterialContainer(const MaterialContainer&)            = delete;
        MaterialContainer(MaterialContainer&&)                 = delete;
        MaterialContainer& operator=(const MaterialContainer&) = delete;
        MaterialContainer& operator=(MaterialContainer&&)      = delete;

        HEDGEHOG_ENGINE_API void Update(const RenderSystem& renderSystem);
        HEDGEHOG_ENGINE_API void SetMaterialDirty(size_t index);

        HEDGEHOG_ENGINE_API void ClearMaterials();
        HEDGEHOG_ENGINE_API void CreateNewMaterial();
        HEDGEHOG_ENGINE_API void SaveMaterial(size_t index);
        HEDGEHOG_ENGINE_API void LoadBaseTexture(size_t index);

        HEDGEHOG_ENGINE_API size_t              GetMaterialCount() const;
        HEDGEHOG_ENGINE_API MaterialData&       GetMaterialDataByIndex(size_t index);
        HEDGEHOG_ENGINE_API const MaterialData& GetMaterialDataByIndex(size_t index) const;

    private:
        const std::string m_DefaultCellTexture = "Textures\\Default\\cells.png";

        std::vector<MaterialData> m_Materials;
    };
}
