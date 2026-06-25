#include "HedgehogEngine/api/Containers/MaterialContainer.hpp"
#include "HedgehogEngine/api/Containers/MaterialData.hpp"
#include "MaterialSerializer.hpp"

#include "DialogueWindows/api/MaterialDialogue.hpp"
#include "DialogueWindows/api/TextureDialogue.hpp"

#include "HedgehogEngine/api/ECS/systems/RenderSystem.hpp"

#include "Logger/api/Logger.hpp"

#include <string_view>

namespace HedgehogEngine
{
    void MaterialContainer::Update(const RenderSystem& renderSystem, const FS::FileSystemManager& fileSystem)
    {
        const auto& materialsInScene = renderSystem.GetMaterials();
        for (size_t i = m_Materials.size(); i < materialsInScene.size(); ++i)
        {
            MaterialData data;
            const std::string virtualPath = "assets://" + materialsInScene[i];
            MaterialSerializer::Deserialize(data, virtualPath, fileSystem);
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

    void MaterialContainer::CreateNewMaterial(const FS::FileSystemManager& fileSystem)
    {
        auto path = DialogueWindows::MaterialCreationDialogue();
        if (path != nullptr)
        {
            const auto virtualPath = fileSystem.ToVirtualPath(path);
            if (!virtualPath)
            {
                LOGERROR("MaterialContainer::CreateNewMaterial: path is not under any registered mount (path: ", path, ")");
                return;
            }

            MaterialData newData;
            newData.type         = MaterialType::Opaque;
            newData.transparency = 1.0f;
            newData.baseColor    = m_DefaultCellTexture;
            // Strip "assets://" prefix to store just the relative path.
            constexpr std::string_view k_AssetsPrefix = "assets://";
            newData.path = virtualPath->substr(k_AssetsPrefix.size());

            MaterialSerializer::Serialize(newData, *virtualPath, fileSystem);
            m_Materials.push_back(newData);
        }
    }

    void MaterialContainer::SaveMaterial(size_t index, const FS::FileSystemManager& fileSystem)
    {
        const MaterialData& data = m_Materials[index];
        const std::string virtualPath = "assets://" + data.path;
        MaterialSerializer::Serialize(data, virtualPath, fileSystem);
    }

    void MaterialContainer::LoadBaseTexture(size_t index, const FS::FileSystemManager& fileSystem)
    {
        auto texturePath = DialogueWindows::TextureOpenDialogue();
        if (texturePath == nullptr)
            return;

        const auto virtualPath = fileSystem.ToVirtualPath(texturePath);
        if (!virtualPath)
        {
            LOGERROR("MaterialContainer::LoadBaseTexture: path is not under any registered mount (path: ", texturePath, ")");
            return;
        }

        // Strip "assets://" prefix; baseColor stores the relative path.
        constexpr std::string_view k_AssetsPrefix = "assets://";
        m_Materials[index].baseColor = virtualPath->substr(k_AssetsPrefix.size());
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
