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
    class LightSystem;
    class RenderSystem;
    class MeshSystem;

    class MeshContainer;
    class TextureContainer;
    class LightContainer;
    class MaterialContainer;

    // Owns the four CPU-side resource containers (mesh/texture/light/material) and implements
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

        HEDGEHOG_ENGINE_API void Update(const ECS::ECS& ecs, const LightSystem& lightSystem,
                                         const RenderSystem& renderSystem, const MeshSystem& meshSystem);

        HEDGEHOG_ENGINE_API const MeshContainer&     GetMeshContainer()     const;
        HEDGEHOG_ENGINE_API const TextureContainer&  GetTextureContainer()  const;
        HEDGEHOG_ENGINE_API TextureContainer&        GetTextureContainer();
        HEDGEHOG_ENGINE_API const LightContainer&    GetLightContainer()    const;
        HEDGEHOG_ENGINE_API const MaterialContainer& GetMaterialContainer() const;
        HEDGEHOG_ENGINE_API MaterialContainer&       GetMaterialContainer();

        // IResourceCatalog — virtual dispatch, no export macro needed across the DLL boundary.
        size_t                       GetMeshCount() const override;
        MeshView                     GetMesh(size_t index) const override;
        size_t                       GetMaterialCount() const override;
        MaterialView                 GetMaterial(size_t index) const override;
        void                         ClearMaterialDirty(size_t index) override;
        void                         RegisterTexturePath(const std::string& path) override;
        const FS::FileSystemManager& GetFileSystem() const override;

    private:
        const FS::FileSystemManager& m_FileSystem;

        std::unique_ptr<MeshContainer>     m_MeshContainer;
        std::unique_ptr<TextureContainer>  m_TextureContainer;
        std::unique_ptr<LightContainer>    m_LightContainer;
        std::unique_ptr<MaterialContainer> m_MaterialContainer;
    };
}
