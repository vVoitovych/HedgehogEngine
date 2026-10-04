#pragma once

#include "EcsSerialization/api/EcsSerializationApi.hpp"
#include "EcsSerialization/api/Reflection/PropertyDescriptor.hpp"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace FS
{
    class FileSystemManager;
}

namespace YAML
{
    class Node;
}

namespace EcsSerialization
{
    // A reference as a file writes it ("Models\a.obj", "assets://Models/a.obj") as a virtual path:
    // backslashes become slashes, a path without a mount goes under assets://, and "." and ".."
    // segments are folded. Empty stays empty.
    [[nodiscard]] ECS_SERIALIZATION_API std::string NormalizeAssetPath(const std::string& written);

    // A path written inside the asset at fromVirtualPath, relative to that asset's folder
    // ("../Pipelines/A.pl" from "engine://x/Shaders/A.shader" is "engine://x/Pipelines/A.pl"). A
    // path that names a mount ("assets://...") is only normalized.
    [[nodiscard]] ECS_SERIALIZATION_API std::string ResolveRelativeAssetPath(const std::string& fromVirtualPath,
                                                                             const std::string& relative);

    // Appends the virtual paths one component's YAML names.
    using ComponentAssetReader = std::function<void(const YAML::Node& component, std::vector<std::string>& out)>;
    // Turns a reflected AssetRef string into a virtual path (NormalizeAssetPath by default).
    using AssetPathNormalizer = std::function<std::string(const std::string& written)>;
    // Appends the virtual paths an asset's text references; returns why it could not be read, or
    // an empty string.
    using AssetFollower =
        std::function<std::string(const std::string& virtualPath, const std::string& text, std::vector<std::string>& out)>;

    // What a scene needs: Assets, every virtual path it reaches, the scene included, sorted and
    // each once; Warnings, the references that do not exist and the assets that could not be
    // read, each naming the asset that referenced it.
    struct AssetDependencies
    {
        std::vector<std::string> Assets;
        std::vector<std::string> Warnings;
    };

    // Collects the closure of a scene's asset references (epic HE-173, for the cook tool). It
    // knows no asset type of its own: the engine registers what each component names (a
    // reflected component's AssetRef properties, or a reader for a hand-written one) and a
    // follower per file extension (a material's textures, a graph's shaders, ...). An asset with
    // no follower references nothing. Each asset is visited once, so reference cycles end.
    class AssetDependencyCollector
    {
    public:
        // Reads the components under componentKey in a scene document; a key registered again
        // gets both readers.
        ECS_SERIALIZATION_API void AddComponentReader(const std::string& componentKey, ComponentAssetReader reader);

        // Reads every AssetRef string property of a reflected component, its default when the
        // document leaves the key out.
        template<typename T>
        void AddReflectedComponent(const std::string& componentKey, AssetPathNormalizer normalize = NormalizeAssetPath)
        {
            const T                  defaults{};
            std::vector<std::string> keys;
            std::vector<std::string> defaultValues;
            for (const Reflection::PropertyDescriptor& property : T::GetProperties())
            {
                if (!Reflection::HasFlag(property.flags, Reflection::PropertyFlags::AssetRef) ||
                    property.type != Reflection::TypeTag::String)
                    continue;
                keys.push_back(property.name);
                defaultValues.push_back(*Reflection::FieldPtr<std::string>(const_cast<T*>(&defaults), property));
            }
            AddReflectedKeys(componentKey, std::move(keys), std::move(defaultValues), std::move(normalize));
        }

        // The follower for files ending in extension (".material"; matched ignoring case).
        ECS_SERIALIZATION_API void AddFollower(const std::string& extension, AssetFollower follower);

        // Appends what a scene document's components name (for a follower of documents that hold
        // entities, such as prefabs).
        ECS_SERIALIZATION_API void ReadSceneReferences(const YAML::Node& document, std::vector<std::string>& out) const;

        // The closure of the scene at sceneVirtualPath, read through fileSystem.
        [[nodiscard]] ECS_SERIALIZATION_API AssetDependencies CollectScene(const std::string&           sceneVirtualPath,
                                                                           const FS::FileSystemManager& fileSystem) const;

    private:
        ECS_SERIALIZATION_API void AddReflectedKeys(const std::string& componentKey, std::vector<std::string> keys,
                                                    std::vector<std::string> defaultValues, AssetPathNormalizer normalize);

        void ReadEntity(const YAML::Node& entity, std::vector<std::string>& out) const;
        const AssetFollower* FindFollower(const std::string& virtualPath) const;

    private:
        std::map<std::string, std::vector<ComponentAssetReader>> m_Readers;
        std::map<std::string, AssetFollower>                     m_Followers;
    };
}
