#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"

#include "HedgehogEngine/api/Containers/MeshContainer.hpp"
#include "HedgehogEngine/api/Containers/Mesh.hpp"
#include "HedgehogEngine/api/Containers/TextureContainer.hpp"
#include "HedgehogEngine/api/Containers/LightContainer.hpp"
#include "HedgehogEngine/api/Containers/MaterialContainer.hpp"
#include "HedgehogEngine/api/Containers/MaterialData.hpp"

namespace HedgehogEngine
{
    ResourceCatalog::ResourceCatalog(const FS::FileSystemManager& fileSystem)
        : m_FileSystem(fileSystem)
        , m_MeshContainer(std::make_unique<MeshContainer>())
        , m_TextureContainer(std::make_unique<TextureContainer>())
        , m_LightContainer(std::make_unique<LightContainer>())
        , m_MaterialContainer(std::make_unique<MaterialContainer>())
    {
    }

    ResourceCatalog::~ResourceCatalog()
    {
    }

    void ResourceCatalog::Update(const ECS::ECS& ecs, const LightSystem& lightSystem,
                                 const RenderSystem& renderSystem, const MeshSystem& meshSystem)
    {
        m_LightContainer->UpdateLights(ecs, lightSystem);
        m_MaterialContainer->Update(renderSystem, m_FileSystem);
        m_MeshContainer->Update(meshSystem, m_FileSystem);
    }

    const MeshContainer& ResourceCatalog::GetMeshContainer() const { return *m_MeshContainer; }
    const TextureContainer& ResourceCatalog::GetTextureContainer() const { return *m_TextureContainer; }
    TextureContainer& ResourceCatalog::GetTextureContainer() { return *m_TextureContainer; }
    const LightContainer& ResourceCatalog::GetLightContainer() const { return *m_LightContainer; }
    const MaterialContainer& ResourceCatalog::GetMaterialContainer() const { return *m_MaterialContainer; }
    MaterialContainer& ResourceCatalog::GetMaterialContainer() { return *m_MaterialContainer; }

    size_t ResourceCatalog::GetMeshCount() const
    {
        return m_MeshContainer->GetMeshCount();
    }

    MeshView ResourceCatalog::GetMesh(size_t index) const
    {
        const Mesh& mesh = m_MeshContainer->GetMesh(index);
        return MeshView
        {
            mesh.GetPositions(),
            mesh.GetNormals(),
            mesh.GetTexCoords(),
            mesh.GetIndices(),
            mesh.GetFirstIndex(),
            mesh.GetIndexCount(),
            mesh.GetVertexOffset()
        };
    }

    size_t ResourceCatalog::GetMaterialCount() const
    {
        return m_MaterialContainer->GetMaterialCount();
    }

    MaterialView ResourceCatalog::GetMaterial(size_t index) const
    {
        const MaterialData& data = m_MaterialContainer->GetMaterialDataByIndex(index);
        return MaterialView{ data.transparency, data.baseColor, data.isDirty };
    }

    void ResourceCatalog::ClearMaterialDirty(size_t index)
    {
        m_MaterialContainer->GetMaterialDataByIndex(index).isDirty = false;
    }

    void ResourceCatalog::RegisterTexturePath(const std::string& path)
    {
        m_TextureContainer->RegisterTexturePath(path);
    }

    const FS::FileSystemManager& ResourceCatalog::GetFileSystem() const
    {
        return m_FileSystem;
    }
}
