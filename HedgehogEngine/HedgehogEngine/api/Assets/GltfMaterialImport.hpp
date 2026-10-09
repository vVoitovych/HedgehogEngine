#pragma once

#include "HedgehogEngine/api/Containers/MaterialData.hpp"
#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "ContentLoader/api/LoadedMaterial.hpp"
#include "FileSystem/api/FileSystemManager.hpp"

#include <string>
#include <vector>

namespace HedgehogEngine
{
    // What ImportGltfMaterials did: Paths holds one .material virtual path per material of the file,
    // in the file's order (so LoadedMesh::MaterialIndex indexes it), Written the ones this import
    // created, and Error why nothing was imported (empty on success).
    struct GltfMaterialImportResult
    {
        std::vector<std::string> Paths;
        std::vector<std::string> Written;
        std::string              Error;
    };

    // The .material a glTF material imports to: <the glTF's folder>/<its stem>_<the material's name>.material,
    // the name's characters other than A-Z, a-z, 0-9, '_' and '-' made '_'.
    [[nodiscard]] HEDGEHOG_ENGINE_API std::string MakeImportedMaterialPath(const std::string& gltfPath,
                                                                           const std::string& materialName);

    // A glTF material as the engine's: its factors, its maps (named under assets:// without the
    // prefix, as the shipped materials name theirs) and an opaque type.
    [[nodiscard]] HEDGEHOG_ENGINE_API MaterialData MakeMaterialData(const ContentLoader::LoadedMaterial& material);

    // Writes one .material per material of a .gltf or .glb file (under assets:// unless it names a
    // mount; ContentLoader's LoadGltfMaterials) beside it. A file that exists already is left
    // untouched, so a material edited after its import keeps its edits; importing again writes nothing.
    // A glTF that does not load gives its reason in Error; a file that cannot be written is logged
    // and left out of Written.
    HEDGEHOG_ENGINE_API GltfMaterialImportResult ImportGltfMaterials(const std::string&           gltfPath,
                                                                    const FS::FileSystemManager& fileSystem);
}
