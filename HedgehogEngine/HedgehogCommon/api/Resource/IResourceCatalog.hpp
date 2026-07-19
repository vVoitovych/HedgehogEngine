#pragma once

#include "HedgehogMath/api/Vector.hpp"

#include <cstddef>
#include <cstdint>
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

        uint32_t firstIndex;
        uint32_t indexCount;
        uint32_t vertexOffset;
    };

    // Zero-copy view onto a single material's CPU-side data.
    struct MaterialView
    {
        float              transparency;
        const std::string& baseColor;
        bool               isDirty;
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

        virtual const FS::FileSystemManager& GetFileSystem() const = 0;
    };
}
