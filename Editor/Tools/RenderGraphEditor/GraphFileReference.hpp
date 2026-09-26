#pragma once

#include <filesystem>
#include <string>

namespace FS
{
    class FileSystemManager;
}

namespace Renderer
{
    class Renderer;
}

namespace Editor
{
    // The graph reference (HedgehogRenderer/Graph/GraphReference.hpp) the editor stores for a
    // .graph file, so every tool names one file the same way and the renderer registers it once:
    // - the file's stem, when the file is in the folder the renderer registers its graphs from;
    // - otherwise its virtual path under the most specific FileSystem mount ("assets://..." rather
    //   than "engine://Assets/...");
    // - otherwise its absolute path.
    // The result is normalized (Renderer::NormalizeGraphReference), as GetGraphNames() lists it.
    [[nodiscard]] std::string MakeGraphReference(const std::filesystem::path& file, const Renderer::Renderer& renderer,
                                                 const FS::FileSystemManager& fileSystem);
}
