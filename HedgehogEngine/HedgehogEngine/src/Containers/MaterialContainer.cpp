#include "HedgehogEngine/api/Containers/MaterialContainer.hpp"
#include "HedgehogEngine/api/Containers/MaterialData.hpp"
#include "MaterialSerializer.hpp"

#include "HedgehogEngine/api/ECS/systems/RenderSystem.hpp"

#include <cassert>
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
        assert(index < m_Materials.size() && "MaterialContainer::SetMaterialDirty: index out of range");
        m_Materials[index].isDirty = true;
    }

    void MaterialContainer::ClearMaterials()
    {
        m_Materials.clear();
    }

    void MaterialContainer::CreateNewMaterial(const FS::FileSystemManager& fileSystem,
                                               const std::string& virtualPath)
    {
        constexpr std::string_view k_Prefix = "assets://";

        MaterialData newData;
        newData.type         = MaterialType::Opaque;
        newData.transparency = 1.0f;
        newData.baseColor    = m_DefaultCellTexture;
        newData.path         = virtualPath.substr(k_Prefix.size());

        MaterialSerializer::Serialize(newData, virtualPath, fileSystem);
        m_Materials.push_back(newData);
    }

    void MaterialContainer::SaveMaterial(size_t index, const FS::FileSystemManager& fileSystem)
    {
        assert(index < m_Materials.size() && "MaterialContainer::SaveMaterial: index out of range");
        const MaterialData& data = m_Materials[index];
        const std::string virtualPath = "assets://" + data.path;
        MaterialSerializer::Serialize(data, virtualPath, fileSystem);
    }

    void MaterialContainer::LoadBaseTexture(size_t index, const std::string& relativePath)
    {
        assert(index < m_Materials.size() && "MaterialContainer::LoadBaseTexture: index out of range");
        m_Materials[index].baseColor = relativePath;
        m_Materials[index].isDirty   = true;
    }

    size_t MaterialContainer::GetMaterialCount() const
    {
        return m_Materials.size();
    }

    MaterialData& MaterialContainer::GetMaterialDataByIndex(size_t index)
    {
        assert(index < m_Materials.size() && "MaterialContainer::GetMaterialDataByIndex: index out of range");
        return m_Materials[index];
    }

    const MaterialData& MaterialContainer::GetMaterialDataByIndex(size_t index) const
    {
        assert(index < m_Materials.size() && "MaterialContainer::GetMaterialDataByIndex: index out of range");
        return m_Materials[index];
    }
}
