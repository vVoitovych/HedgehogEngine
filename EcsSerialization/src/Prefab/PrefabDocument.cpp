#include "EcsSerialization/api/Prefab/PrefabDocument.hpp"
#include "EcsSerialization/api/EcsSerializer.hpp"

namespace EcsSerialization
{
    std::string WritePrefab(const ComponentSerializerRegistry& registry, const ECS::ECS& ecs, ECS::Entity subtreeRoot)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Version" << YAML::Value << PREFAB_FORMAT_VERSION;
        out << YAML::Key << "Root" << YAML::Value << EcsSerializer::SerializeSubtree(registry, ecs, subtreeRoot);
        out << YAML::EndMap;
        return std::string(out.c_str()) + "\n";
    }

    PrefabReadResult ReadPrefab(const std::string& text)
    {
        PrefabReadResult result;
        YAML::Node       document;
        try
        {
            document = YAML::Load(text);
        }
        catch (const YAML::Exception& e)
        {
            result.Error = std::string("not valid YAML: ") + e.what();
            return result;
        }
        if (!document.IsMap())
        {
            result.Error = "not a prefab document";
            return result;
        }

        int version = 0;
        if (!document["Version"] || !YAML::convert<int>::decode(document["Version"], version) || version < 1)
        {
            result.Error = "Version is missing or is not a positive integer";
            return result;
        }
        if (version > PREFAB_FORMAT_VERSION)
        {
            result.Error = "the prefab is format version " + std::to_string(version) + ", but this build reads up to version " +
                           std::to_string(PREFAB_FORMAT_VERSION);
            return result;
        }

        const YAML::Node root = document["Root"];
        if (!root || !root.IsMap() || !root["Subtree"] || !root["Subtree"].IsSequence())
        {
            result.Error = "Root is missing or holds no Subtree";
            return result;
        }
        result.Root = root;
        return result;
    }
}
