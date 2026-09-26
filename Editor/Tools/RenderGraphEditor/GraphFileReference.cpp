#include "GraphFileReference.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "HedgehogRenderer/Graph/GraphReference.hpp"
#include "HedgehogRenderer/Renderer.hpp"

#include <algorithm>
#include <iterator>

namespace Editor
{
    namespace
    {
        constexpr const char* DIALOGUE_FOLDER = "assets://";

        // Absolute, with '..' resolved and the real spelling of the parts that exist, and no
        // trailing separator, so two paths to one place compare equal element by element.
        std::filesystem::path Canonical(const std::filesystem::path& path)
        {
            std::error_code error;
            std::filesystem::path result = std::filesystem::weakly_canonical(path, error);
            if (error)
                result = path;
            result = result.lexically_normal();
            if (!result.has_filename() && result.has_parent_path())
                result = result.parent_path();
            return result;
        }
    }

    std::string MakeGraphReference(const std::filesystem::path& file, const Renderer::Renderer& renderer,
                                   const FS::FileSystemManager& fileSystem)
    {
        const std::filesystem::path path = Canonical(file);

        for (const std::string& name : renderer.GetGraphNames())
        {
            if (Renderer::ClassifyGraphReference(name) == Renderer::GraphReferenceKind::Name
                && Canonical(renderer.GetGraphFile(name)).parent_path() == path.parent_path())
            {
                return path.stem().string();
            }
        }

        std::string best;
        ptrdiff_t   bestDepth = 0;
        for (const auto& mounted : fileSystem.GetFileSystems())
        {
            for (const auto& [alias, root] : mounted->GetMountPoints())
            {
                const std::filesystem::path mount = Canonical(root);
                const auto [mountEnd, pathEnd] = std::mismatch(mount.begin(), mount.end(), path.begin(), path.end());
                const ptrdiff_t depth = std::distance(mount.begin(), mount.end());
                if (mountEnd == mount.end() && depth > bestDepth)
                {
                    bestDepth = depth;
                    best      = alias + path.lexically_relative(mount).generic_string();
                }
            }
        }
        // The renderer's spelling of the reference, so the editor can compare it with GetGraphNames().
        return Renderer::NormalizeGraphReference(best.empty() ? path.generic_string() : best);
    }

    std::string GetGraphDialoguePath(const FS::FileSystemManager& fileSystem, const char* fileName)
    {
        const std::optional<std::filesystem::path> folder = fileSystem.ResolvePhysical(DIALOGUE_FOLDER);
        return folder ? (*folder / fileName).make_preferred().string() : std::string(fileName);
    }
}
