#include "EcsSerialization/api/Assets/AssetDependencyCollector.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>
#include <set>

namespace EcsSerialization
{
    namespace
    {
        constexpr std::string_view MOUNT_SEPARATOR = "://";
        constexpr std::string_view DEFAULT_MOUNT   = "assets://";

        std::string ToLower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text;
        }

        std::string Fold(const std::string& path)
        {
            return std::filesystem::path(path).lexically_normal().generic_string();
        }

        bool IsAbsolute(const std::string& path)
        {
            return (path.size() > 1 && path[1] == ':') || (!path.empty() && path.front() == '/');
        }

        // Where an asset's reference came from, for a warning.
        struct PendingAsset
        {
            std::string Path;
            std::string From;
            bool        IsScene = false;
        };

        std::string Describe(const PendingAsset& asset)
        {
            return asset.From.empty() ? asset.Path : asset.Path + " (referenced by " + asset.From + ")";
        }
    }

    std::string NormalizeAssetPath(const std::string& written)
    {
        if (written.empty())
            return {};
        std::string path = written;
        std::replace(path.begin(), path.end(), '\\', '/');

        const size_t separator = path.find(MOUNT_SEPARATOR);
        if (separator != std::string::npos)
        {
            const size_t rest = separator + MOUNT_SEPARATOR.size();
            return path.substr(0, rest) + Fold(path.substr(rest));
        }
        if (IsAbsolute(path))
            return Fold(path);
        return std::string(DEFAULT_MOUNT) + Fold(path);
    }

    std::string ResolveRelativeAssetPath(const std::string& fromVirtualPath, const std::string& relative)
    {
        if (relative.find(MOUNT_SEPARATOR) != std::string::npos)
            return NormalizeAssetPath(relative);
        std::string from = fromVirtualPath;
        std::replace(from.begin(), from.end(), '\\', '/');
        const size_t slash = from.rfind('/');
        return NormalizeAssetPath(from.substr(0, slash == std::string::npos ? 0 : slash + 1) + relative);
    }

    void AssetDependencyCollector::AddComponentReader(const std::string& componentKey, ComponentAssetReader reader)
    {
        m_Readers[componentKey].push_back(std::move(reader));
    }

    void AssetDependencyCollector::AddReflectedKeys(const std::string& componentKey, std::vector<std::string> keys,
                                                    std::vector<std::string> defaultValues, AssetPathNormalizer normalize)
    {
        AddComponentReader(componentKey,
                           [keys = std::move(keys), defaultValues = std::move(defaultValues),
                            normalize = std::move(normalize)](const YAML::Node& component, std::vector<std::string>& out)
                           {
                               for (size_t i = 0; i < keys.size(); ++i)
                               {
                                   const YAML::Node  value   = component[keys[i]];
                                   const std::string written = value ? value.as<std::string>() : defaultValues[i];
                                   if (std::string path = normalize(written); !path.empty())
                                       out.push_back(std::move(path));
                               }
                           });
    }

    void AssetDependencyCollector::AddFollower(const std::string& extension, AssetFollower follower)
    {
        m_Followers[ToLower(extension)] = std::move(follower);
    }

    const AssetFollower* AssetDependencyCollector::FindFollower(const std::string& virtualPath) const
    {
        const size_t dot   = virtualPath.rfind('.');
        const size_t slash = virtualPath.rfind('/');
        if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
            return nullptr;
        const auto it = m_Followers.find(ToLower(virtualPath.substr(dot)));
        return it == m_Followers.end() ? nullptr : &it->second;
    }

    void AssetDependencyCollector::ReadEntity(const YAML::Node& entity, std::vector<std::string>& out) const
    {
        if (!entity.IsMap())
            return;
        for (const auto& entry : entity)
        {
            const std::string key = entry.first.as<std::string>();
            if (key == "Children" && entry.second.IsSequence())
            {
                for (const YAML::Node& child : entry.second)
                    ReadEntity(child, out);
                continue;
            }
            // A prefab instance's root names its prefab, whose own entities are not in the scene.
            if (key == "Prefab" && entry.second.IsScalar())
            {
                if (std::string path = NormalizeAssetPath(entry.second.as<std::string>()); !path.empty())
                    out.push_back(std::move(path));
                continue;
            }
            const auto readers = m_Readers.find(key);
            if (readers == m_Readers.end() || !entry.second.IsMap())
                continue;
            for (const ComponentAssetReader& reader : readers->second)
                reader(entry.second, out);
        }
    }

    void AssetDependencyCollector::ReadSceneReferences(const YAML::Node& document, std::vector<std::string>& out) const
    {
        // A scene's entities, or a subtree's (a prefab's Root).
        for (const char* key : { "Scene", "Subtree" })
        {
            const YAML::Node entities = document[key];
            if (!entities || !entities.IsSequence())
                continue;
            for (const YAML::Node& entity : entities)
                ReadEntity(entity, out);
        }
    }

    AssetDependencies AssetDependencyCollector::CollectScene(const std::string&           sceneVirtualPath,
                                                             const FS::FileSystemManager& fileSystem) const
    {
        return Collect({ sceneVirtualPath }, {}, fileSystem);
    }

    AssetDependencies AssetDependencyCollector::Collect(const std::vector<std::string>& scenes,
                                                        const std::vector<std::string>& assets,
                                                        const FS::FileSystemManager&    fileSystem) const
    {
        AssetDependencies         result;
        std::set<std::string>     visited;
        std::vector<PendingAsset> pending;
        for (auto it = assets.rbegin(); it != assets.rend(); ++it)
            pending.push_back({ NormalizeAssetPath(*it), {}, false });
        for (auto it = scenes.rbegin(); it != scenes.rend(); ++it)
            pending.push_back({ NormalizeAssetPath(*it), {}, true });
        while (!pending.empty())
        {
            const PendingAsset asset = std::move(pending.back());
            pending.pop_back();
            if (asset.Path.empty() || !visited.insert(asset.Path).second)
                continue;
            if (!fileSystem.Exists(asset.Path))
            {
                result.Warnings.push_back(Describe(asset) + " does not exist.");
                continue;
            }
            result.Assets.push_back(asset.Path);

            const bool           isScene  = asset.IsScene;
            const AssetFollower* follower = isScene ? nullptr : FindFollower(asset.Path);
            if (!isScene && !follower)
                continue;
            const std::optional<std::string> text = fileSystem.ReadTextFile(asset.Path);
            if (!text)
            {
                result.Warnings.push_back(Describe(asset) + " cannot be read.");
                continue;
            }

            std::vector<std::string> references;
            std::string              error;
            try
            {
                if (isScene)
                    ReadSceneReferences(YAML::Load(*text), references);
                else
                    error = (*follower)(asset.Path, *text, references);
            }
            catch (const std::exception& e)
            {
                error = e.what();
            }
            if (!error.empty())
                result.Warnings.push_back(Describe(asset) + ": " + error);

            for (std::string& reference : references)
                pending.push_back({ std::move(reference), asset.Path, false });
        }
        std::sort(result.Assets.begin(), result.Assets.end());
        return result;
    }
}
