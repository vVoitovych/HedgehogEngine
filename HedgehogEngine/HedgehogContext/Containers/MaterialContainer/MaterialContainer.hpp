#pragma once

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

        void Update(const Scene::Scene& scene);
        void SetMaterialDirty(size_t index);

        void ClearMaterials();
        void CreateNewMaterial();
        void SaveMaterial(size_t index);
        void LoadBaseTexture(size_t index);

        size_t              GetMaterialCount() const;
        MaterialData&       GetMaterialDataByIndex(size_t index);
        const MaterialData& GetMaterialDataByIndex(size_t index) const;

    private:
        const std::string m_DefaultCellTexture = "Textures\\Default\\cells.png";

        std::vector<MaterialData> m_Materials;
    };
}
