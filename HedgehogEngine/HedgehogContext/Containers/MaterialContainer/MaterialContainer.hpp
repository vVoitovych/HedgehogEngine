#pragma once

#include "HedgehogContext/api/HedgehogContextApi.hpp"

#include <string>
#include <vector>

namespace Scene
{
    class Scene;
}

namespace Context
{
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

        HEDGEHOG_CONTEXT_API void Update(const Scene::Scene& scene);
        HEDGEHOG_CONTEXT_API void SetMaterialDirty(size_t index);

        HEDGEHOG_CONTEXT_API void ClearMaterials();
        HEDGEHOG_CONTEXT_API void CreateNewMaterial();
        HEDGEHOG_CONTEXT_API void SaveMaterial(size_t index);
        HEDGEHOG_CONTEXT_API void LoadBaseTexture(size_t index);

        HEDGEHOG_CONTEXT_API size_t              GetMaterialCount() const;
        HEDGEHOG_CONTEXT_API MaterialData&       GetMaterialDataByIndex(size_t index);
        HEDGEHOG_CONTEXT_API const MaterialData& GetMaterialDataByIndex(size_t index) const;

    private:
        const std::string m_DefaultCellTexture = "Textures\\Default\\cells.png";

        std::vector<MaterialData> m_Materials;
    };
}
