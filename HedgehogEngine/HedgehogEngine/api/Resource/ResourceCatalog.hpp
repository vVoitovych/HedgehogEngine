#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"
#include "HedgehogCommon/api/Resource/IResourceCatalog.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <memory>
#include <string>

namespace ECS
{
    class ECS;
}

namespace HedgehogEngine
{
    class RenderSystem;
    class MeshSystem;

    class MeshContainer;
    class TextureContainer;
    class MaterialContainer;
    class FontContainer;
    class TerrainContainer;

    // Owns the three CPU-side resource containers (mesh/texture/material) and implements
    // IResourceCatalog, the narrow read-mostly contract consumed by the renderer. The Editor
    // uses the concrete accessors below when it needs container-specific operations (dialogues,
    // material editing) rather than the interface's plain-data views.
    class ResourceCatalog : public IResourceCatalog
    {
    public:
        HEDGEHOG_ENGINE_API explicit ResourceCatalog(const FS::FileSystemManager& fileSystem);
        HEDGEHOG_ENGINE_API ~ResourceCatalog() override;

        ResourceCatalog(const ResourceCatalog&)            = delete;
        ResourceCatalog& operator=(const ResourceCatalog&) = delete;
        ResourceCatalog(ResourceCatalog&&)                 = delete;
        ResourceCatalog& operator=(ResourceCatalog&&)      = delete;

        // Loads the materials and meshes the systems have listed since the last call. The two are
        // independent: MeshSystem and RenderSystem call their own half in the Sync phase each frame;
        // Update does both at once, for callers outside the frame (a scene just loaded, a new asset).
        HEDGEHOG_ENGINE_API void Update(const RenderSystem& renderSystem, const MeshSystem& meshSystem);
        HEDGEHOG_ENGINE_API void UpdateMaterials(const RenderSystem& renderSystem);
        // Also rebuilds, in place, the mesh of every terrain whose heights changed.
        HEDGEHOG_ENGINE_API void UpdateMeshes(const MeshSystem& meshSystem);

        HEDGEHOG_ENGINE_API const MeshContainer&     GetMeshContainer()     const;
        HEDGEHOG_ENGINE_API const TextureContainer&  GetTextureContainer()  const;
        HEDGEHOG_ENGINE_API TextureContainer&        GetTextureContainer();
        HEDGEHOG_ENGINE_API const MaterialContainer& GetMaterialContainer() const;
        HEDGEHOG_ENGINE_API MaterialContainer&       GetMaterialContainer();
        HEDGEHOG_ENGINE_API const FontContainer&     GetFontContainer()     const;
        HEDGEHOG_ENGINE_API FontContainer&           GetFontContainer();
        HEDGEHOG_ENGINE_API const TerrainContainer&  GetTerrainContainer()  const;
        HEDGEHOG_ENGINE_API TerrainContainer&        GetTerrainContainer();

        // IResourceCatalog — virtual dispatch, no export macro needed across the DLL boundary.
        size_t                       GetMeshCount() const override;
        MeshView                     GetMesh(size_t index) const override;
        uint32_t                     GetMeshVersion(size_t index) const override;
        size_t                       GetMaterialCount() const override;
        MaterialView                 GetMaterial(size_t index) const override;
        void                         ClearMaterialDirty(size_t index) override;
        void                         RegisterTexturePath(const std::string& path) override;
        const FS::FileSystemManager& GetFileSystem() const override;
        size_t                       GetFontCount() const override;
        FontAtlasView                GetFontAtlas(size_t index) const override;
        size_t                       GetTerrainCount() const override;
        TerrainView                  GetTerrain(size_t index) const override;
        void                         ClearTerrainDirty(size_t index, bool splat, bool layers) override;

    private:
        const FS::FileSystemManager& m_FileSystem;

        std::unique_ptr<MeshContainer>     m_MeshContainer;
        std::unique_ptr<TextureContainer>  m_TextureContainer;
        std::unique_ptr<MaterialContainer> m_MaterialContainer;
        std::unique_ptr<FontContainer>     m_FontContainer;
        std::unique_ptr<TerrainContainer>  m_TerrainContainer;
    };
}
