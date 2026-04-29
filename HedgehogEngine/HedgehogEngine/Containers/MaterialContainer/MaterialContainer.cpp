#include "MaterialContainer.hpp"
#include "MaterialData.hpp"
#include "MaterialSerializer.hpp"

#include "DialogueWindows/api/MaterialDialogue.hpp"
#include "DialogueWindows/api/TextureDialogue.hpp"

#include "ContentLoader/api/CommonFunctions.hpp"

#include "Components/api/RenderSystem.hpp"

namespace HedgehogEngine
{
    void MaterialContainer::Update(const Scene::RenderSystem& renderSystem)
    {
        const auto& materialsInScene = renderSystem.GetMaterials();
        for (size_t i = m_Materials.size(); i < materialsInScene.size(); ++i)
        {
            MaterialData data;
            MaterialSerializer::Deserialize(data, ContentLoader::GetAssetsDirectory() + materialsInScene[i]);
            m_Materials.push_back(data);
        }
    }

    void MaterialContainer::SetMaterialDirty(size_t index)
    {
        m_Materials[index].isDirty = true;
    }

    void MaterialContainer::ClearMaterials()
    {
        m_Materials.clear();
    }

    void MaterialContainer::CreateNewMaterial()
    {
        auto path = DialogueWindows::MaterialCreationDialogue();
        if (path != nullptr)
        {
            MaterialData newData;
            newData.type         = MaterialType::Opaque;
            newData.transparency = 1.0f;
            newData.baseColor    = m_DefaultCellTexture;
            newData.path         = ContentLoader::GetAssetRelativetlyPath(path);

            MaterialSerializer::Serialize(newData, path);
            m_Materials.push_back(newData);
        }
    }

    void MaterialContainer::SaveMaterial(size_t index)
    {
        const MaterialData& data = m_Materials[index];
        MaterialSerializer::Serialize(data, ContentLoader::GetAssetsDirectory() + data.path);
    }

    void MaterialContainer::LoadBaseTexture(size_t index)
    {
        auto texturePath = DialogueWindows::TextureOpenDialogue();
        if (texturePath == nullptr)
            return;
        m_Materials[index].baseColor = ContentLoader::GetAssetRelativetlyPath(texturePath);
        m_Materials[index].isDirty   = true;
    }

    size_t MaterialContainer::GetMaterialCount() const
    {
        return m_Materials.size();
    }

    MaterialData& MaterialContainer::GetMaterialDataByIndex(size_t index)
    {
        return m_Materials[index];
    }

    const MaterialData& MaterialContainer::GetMaterialDataByIndex(size_t index) const
    {
        return m_Materials[index];
    }
}
