#pragma once

#include "HedgehogMath/api/Vector.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace FS
{
    class FileSystemManager;
}

namespace HedgehogEngine
{
    // Zero-copy view onto a single mesh's CPU-side geometry, valid only for the
    // duration of the call that produced it (references into the owning container).
    struct MeshView
    {
        const std::vector<HM::Vector3>& positions;
        const std::vector<HM::Vector3>& normals;
        const std::vector<HM::Vector2>& texCoords;
        const std::vector<uint32_t>&    indices;

        // A skinned mesh's per-vertex joint indices and weights; both empty for a static mesh.
        const std::vector<HM::Vector4u>& joints;
        const std::vector<HM::Vector4>&  weights;

        uint32_t firstIndex;
        uint32_t indexCount;
        uint32_t vertexOffset;
    };

    // Zero-copy view onto a single material's CPU-side data.
    // A material's values (HedgehogEngine's MaterialData) for the renderer; map paths are empty for
    // none.
    struct MaterialView
    {
        float              transparency;
        const std::string& baseColor; // the base colour map
        bool               isDirty;

        HM::Vector4        baseColorFactor;
        float              metallic;
        float              roughness;
        const std::string& metallicRoughnessMap;
        const std::string& normalMap;
        float              normalScale;
        const std::string& occlusionMap;
        float              occlusionStrength;
        const std::string& emissiveMap;
        HM::Vector3        emissiveFactor;
    };

    // Zero-copy view onto a baked font's coverage atlas: AtlasWidth x AtlasHeight bytes, row by row.
    struct FontAtlasView
    {
        uint32_t                 AtlasWidth  = 0;
        uint32_t                 AtlasHeight = 0;
        std::span<const uint8_t> Atlas;
    };

    // Read-mostly contract the renderer consumes to sync its GPU-side mesh/material/texture
    // resources, without depending on the concrete HedgehogEngine containers.
    class IResourceCatalog
    {
    public:
        virtual ~IResourceCatalog() = default;

        virtual size_t    GetMeshCount() const = 0;
        virtual MeshView   GetMesh(size_t index) const = 0;

        virtual size_t        GetMaterialCount() const = 0;
        virtual MaterialView  GetMaterial(size_t index) const = 0;
        virtual void          ClearMaterialDirty(size_t index) = 0;

        virtual void RegisterTexturePath(const std::string& path) = 0;

        // Fonts baked so far, by the index the UI's draw commands name (HX::RenderScene::UiFonts).
        // Fonts are only ever appended.
        virtual size_t        GetFontCount() const = 0;
        virtual FontAtlasView GetFontAtlas(size_t index) const = 0;

        virtual const FS::FileSystemManager& GetFileSystem() const = 0;
    };
}
