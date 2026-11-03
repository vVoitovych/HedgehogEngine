#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"

#include "HedgehogEngine/api/Containers/MeshContainer.hpp"
#include "HedgehogEngine/api/Containers/Mesh.hpp"
#include "HedgehogEngine/api/Containers/TextureContainer.hpp"
#include "HedgehogEngine/api/Containers/MaterialContainer.hpp"
#include "HedgehogEngine/api/Containers/MaterialData.hpp"
#include "HedgehogEngine/api/Containers/FontContainer.hpp"
#include "HedgehogEngine/api/Containers/TerrainContainer.hpp"
#include "HedgehogEngine/api/Terrain/TerrainGeometry.hpp"

#include "Logger/api/Logger.hpp"

namespace HedgehogEngine
{
    ResourceCatalog::ResourceCatalog(const FS::FileSystemManager& fileSystem)
        : m_FileSystem(fileSystem)
        , m_MeshContainer(std::make_unique<MeshContainer>())
        , m_TextureContainer(std::make_unique<TextureContainer>())
        , m_MaterialContainer(std::make_unique<MaterialContainer>())
        , m_FontContainer(std::make_unique<FontContainer>(fileSystem))
        , m_TerrainContainer(std::make_unique<TerrainContainer>(fileSystem))
    {
    }

    ResourceCatalog::~ResourceCatalog()
    {
    }

    void ResourceCatalog::Update(const RenderSystem& renderSystem, const MeshSystem& meshSystem)
    {
        UpdateMaterials(renderSystem);
        UpdateMeshes(meshSystem);
    }

    void ResourceCatalog::UpdateMaterials(const RenderSystem& renderSystem)
    {
        m_MaterialContainer->Update(renderSystem, m_FileSystem);
    }

    void ResourceCatalog::UpdateMeshes(const MeshSystem& meshSystem)
    {
        m_MeshContainer->Update(meshSystem, m_FileSystem, m_TerrainContainer.get());

        for (size_t terrain = 0; terrain < m_TerrainContainer->GetTerrainCount(); ++terrain)
        {
            if (!HasAny(m_TerrainContainer->GetDirty(terrain), TerrainDirty::Heights))
                continue;
            const std::optional<size_t> mesh =
                m_MeshContainer->FindMesh(MakeTerrainMeshPath(m_TerrainContainer->GetPath(terrain)));
            if (!mesh)
                continue; // no entity draws it yet
            // A tile keeps its resolution while it is sculpted, so its grid fits its slot.
            if (!m_MeshContainer->ReplaceGeometry(*mesh, BuildTerrainMesh(m_TerrainContainer->GetTerrain(terrain))))
                LOGWARNING("[Terrain]", m_TerrainContainer->GetPath(terrain),
                           "changed resolution; its mesh keeps its old grid until the scene is opened again.");
            m_TerrainContainer->ClearDirty(terrain, TerrainDirty::Heights);
        }
    }

    const MeshContainer& ResourceCatalog::GetMeshContainer() const { return *m_MeshContainer; }
    const TextureContainer& ResourceCatalog::GetTextureContainer() const { return *m_TextureContainer; }
    TextureContainer& ResourceCatalog::GetTextureContainer() { return *m_TextureContainer; }
    const MaterialContainer& ResourceCatalog::GetMaterialContainer() const { return *m_MaterialContainer; }
    MaterialContainer& ResourceCatalog::GetMaterialContainer() { return *m_MaterialContainer; }
    const FontContainer& ResourceCatalog::GetFontContainer() const { return *m_FontContainer; }
    FontContainer& ResourceCatalog::GetFontContainer() { return *m_FontContainer; }
    const TerrainContainer& ResourceCatalog::GetTerrainContainer() const { return *m_TerrainContainer; }
    TerrainContainer& ResourceCatalog::GetTerrainContainer() { return *m_TerrainContainer; }

    size_t ResourceCatalog::GetMeshCount() const
    {
        return m_MeshContainer->GetMeshCount();
    }

    uint32_t ResourceCatalog::GetMeshVersion(size_t index) const
    {
        return m_MeshContainer->GetMeshVersion(index);
    }

    MeshView ResourceCatalog::GetMesh(size_t index) const
    {
        const Mesh& mesh = m_MeshContainer->GetMesh(index);
        return MeshView
        {
            mesh.GetPositions(),
            mesh.GetNormals(),
            mesh.GetTexCoords(),
            mesh.GetTangents(),
            mesh.GetIndices(),
            mesh.GetJoints(),
            mesh.GetWeights(),
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
        return MaterialView{ data.transparency, data.baseColor,         data.isDirty,
                             data.baseColorFactor, data.metallic,        data.roughness,
                             data.metallicRoughnessMap, data.normalMap,  data.normalScale,
                             data.occlusionMap, data.occlusionStrength, data.emissiveMap,
                             data.emissiveFactor,
                             static_cast<MaterialAlphaMode>(data.type), data.alphaCutoff,
                             data.doubleSided };
    }

    void ResourceCatalog::ClearMaterialDirty(size_t index)
    {
        m_MaterialContainer->GetMaterialDataByIndex(index).isDirty = false;
    }

    void ResourceCatalog::RegisterTexturePath(const std::string& path)
    {
        m_TextureContainer->RegisterTexturePath(path);
    }

    size_t ResourceCatalog::GetFontCount() const
    {
        return m_FontContainer->GetFontCount();
    }

    FontAtlasView ResourceCatalog::GetFontAtlas(size_t index) const
    {
        const ContentLoader::LoadedFont& font = m_FontContainer->GetFont(index);
        return FontAtlasView{ font.AtlasWidth, font.AtlasHeight, font.Atlas };
    }

    const FS::FileSystemManager& ResourceCatalog::GetFileSystem() const
    {
        return m_FileSystem;
    }
}
