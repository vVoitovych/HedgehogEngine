#pragma once

#include "ContentLoaderApi.hpp"
#include "LoadedMaterial.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <optional>
#include <string>
#include <vector>

namespace ContentLoader
{
    // The materials of a .gltf or .glb file (under assets:// unless fileName names a mount), in the
    // file's order, so LoadedMesh::MaterialIndex indexes them. A map's image is named by its
    // virtual path next to the file ("Models/Helmet/albedo.jpg" for "Models/Helmet/Helmet.gltf");
    // an image embedded in the file (a data: URI or a buffer view) is skipped with one warning naming
    // the material and slot, and a map reading texture coordinates other than the first is kept with
    // one warning. Returns std::nullopt (after logging) when the path cannot be resolved, the format
    // is unsupported, or the file fails to parse.
    CONTENT_LOADER_API std::optional<std::vector<LoadedMaterial>> LoadGltfMaterials(const std::string& fileName,
                                                                                   const FS::FileSystemManager& fileSystem);
}
