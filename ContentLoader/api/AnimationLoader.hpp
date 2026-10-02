#pragma once

#include "ContentLoaderApi.hpp"
#include "LoadedAnimation.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <optional>
#include <string>
#include <vector>

namespace ContentLoader
{
    // The animation clips of a .gltf or .glb file under assets://, their channels addressed by the
    // joint indices LoadMesh gives the same file's skin. Channels that target a node outside the
    // skin, and morph-target weights, are skipped. A file without a skin has no clips. Returns
    // std::nullopt (after logging) when the path cannot be resolved, the format is unsupported,
    // or the file fails to parse.
    CONTENT_LOADER_API std::optional<std::vector<LoadedAnimationClip>> LoadAnimations(
        const std::string& fileName, const FS::FileSystemManager& fileSystem);
}
