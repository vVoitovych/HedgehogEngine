#include "EcsSerialization/api/Prefab/PrefabExpansion.hpp"
#include "EcsSerialization/api/ComponentSerializerRegistry.hpp"
#include "EcsSerialization/api/Prefab/IPrefabProvider.hpp"
#include "EcsSerialization/api/Prefab/OverrideSet.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <set>

namespace EcsSerialization
{
    namespace
    {
        // Source ids given to entities a nested prefab gained since the outer one was written: far
        // above any real entity id, so no reference in the documents names one by chance.
        constexpr ECS::Entity FIRST_SYNTHETIC_SOURCE = ECS::Entity(1) << 40;

        bool HoldsInstances(const YAML::Node& node)
        {
            if (node["Prefab"])
                return true;
            for (const YAML::Node& child : node["Children"])
            {
                if (HoldsInstances(child))
                    return true;
            }
            return false;
        }

        void CollectNested(const YAML::Node& node, std::vector<std::string>& out)
        {
            if (const YAML::Node prefab = node["Prefab"])
            {
                const std::string path = prefab.as<std::string>();
                if (std::find(out.begin(), out.end(), path) == out.end())
                    out.push_back(path);
            }
            for (const YAML::Node& child : node["Children"])
                CollectNested(child, out);
        }

        // The outer document's ids: local id to source id, grown for entities a nested prefab gained.
        struct IdSpace
        {
            std::vector<ECS::Entity> SourceIds;
            ECS::Entity              NextSynthetic = FIRST_SYNTHETIC_SOURCE;

            ECS::Entity AddLocal()
            {
                SourceIds.push_back(NextSynthetic++);
                return SourceIds.size() - 1;
            }
        };

        struct Expansion
        {
            IPrefabProvider&                   Prefabs;
            const ComponentSerializerRegistry& Registry;
            std::vector<std::string>           Chain;
            std::string                        Error;
        };

        std::optional<YAML::Node> Expand(Expansion& expansion, const std::string& path);
        YAML::Node                ExpandNode(Expansion& expansion, const YAML::Node& node, IdSpace& space);

        // A nested prefab's tree renumbered into the outer ids, its references rewritten.
        struct Renumbering
        {
            const ComponentSerializerRegistry&  Registry;
            ECS::Entity                         InstanceNode;    // the instance node's outer id
            std::vector<ECS::Entity>            ToOuter;         // nested id to outer id
            EntityRemap                         Sources;         // nested source id to outer source id
            std::map<ECS::Entity, YAML::Node>   ByNested;        // the renumbered nodes
            std::map<ECS::Entity, YAML::Node>   ByOuter;
        };

        YAML::Node Renumber(Renumbering& renumbering, const YAML::Node& node, ECS::Entity outerParent)
        {
            const ECS::Entity nested = node["Entity"].as<ECS::Entity>();
            const ECS::Entity outer  = renumbering.ToOuter.at(nested);
            YAML::Node        out(YAML::NodeType::Map);
            for (const auto& entry : node)
            {
                const std::string key = entry.first.Scalar();
                if (key == "Children" || key == "PrefabOrigin")
                    continue;
                if (key == "Entity")
                    out[key] = outer;
                else if (key == "Parent")
                    out[key] = outerParent;
                else
                    out[key] = YAML::Clone(entry.second);
            }
            for (const auto& handler : renumbering.Registry.GetHandlers())
            {
                if (!handler.RemapYaml || !out[handler.YamlKey])
                    continue;
                YAML::Node component = out[handler.YamlKey];
                handler.RemapYaml(component, renumbering.Sources);
            }
            YAML::Node origin(YAML::NodeType::Sequence);
            origin.push_back(renumbering.InstanceNode);
            origin.push_back(nested);
            origin.SetStyle(YAML::EmitterStyle::Flow);
            out["PrefabOrigin"] = origin;

            YAML::Node children(YAML::NodeType::Sequence);
            for (const YAML::Node& child : node["Children"])
                children.push_back(Renumber(renumbering, child, outer));
            out["Children"] = children;
            renumbering.ByNested[nested] = out;
            renumbering.ByOuter[outer]   = out;
            return out;
        }

        // An instance node: its prefab expanded and renumbered, overridden and given its added entities.
        YAML::Node ExpandInstance(Expansion& expansion, const YAML::Node& instance, IdSpace& space)
        {
            const std::string               path   = instance["Prefab"].as<std::string>();
            const std::optional<YAML::Node> nested = Expand(expansion, path);
            if (!nested)
                return {}; // Expand set the error

            const auto        nestedSources = (*nested)["SourceIds"].as<std::vector<ECS::Entity>>();
            const ECS::Entity instanceId    = instance["Entity"].as<ECS::Entity>();
            std::vector<ECS::Entity> saved;
            if (const YAML::Node entities = instance["Entities"]; entities && entities.IsSequence())
                saved = entities.as<std::vector<ECS::Entity>>();

            Renumbering renumbering{ expansion.Registry, instanceId, {}, {}, {}, {} };
            renumbering.ToOuter.assign(nestedSources.size(), ECS::INVALID_ENTITY);
            for (size_t id = 0; id < nestedSources.size(); ++id)
            {
                if (id == 0)
                    renumbering.ToOuter[id] = instanceId;
                else if (id < saved.size() && saved[id] < space.SourceIds.size())
                    renumbering.ToOuter[id] = saved[id];
                else
                    renumbering.ToOuter[id] = space.AddLocal();
            }
            renumbering.Sources = [&nestedSources, &renumbering, &space](ECS::Entity source)
            {
                for (size_t id = 0; id < nestedSources.size(); ++id)
                {
                    if (nestedSources[id] == source)
                        return space.SourceIds[renumbering.ToOuter[id]];
                }
                return ECS::INVALID_ENTITY;
            };

            YAML::Node root = Renumber(renumbering, (*nested)["Subtree"][0], instance["Parent"].as<ECS::Entity>());
            root["Name"]    = instance["Name"].as<std::string>();

            // The overrides, written in the outer ids, onto the nested entities they name.
            const std::string what = "instance '" + instance["Name"].as<std::string>() + "' of " + path;
            for (const PropertyOverride& entry : ReadOverrides(instance["Overrides"], what))
            {
                const auto node = renumbering.ByNested.find(entry.LocalId);
                if (node == renumbering.ByNested.end())
                {
                    LOGWARNING("[Prefab] " + what + ": the override of " + entry.Component + " on node " +
                               std::to_string(entry.LocalId) + " is dropped: the prefab has no such node.");
                    continue;
                }
                YAML::Node target = node->second;
                if (entry.Property.empty())
                    target[entry.Component] = YAML::Clone(entry.Value);
                else if (target[entry.Component])
                    target[entry.Component][entry.Property] = YAML::Clone(entry.Value);
            }

            // The added entities, under the entity they hang from (the instance's root when it is gone).
            for (const YAML::Node& child : instance["Children"])
            {
                YAML::Node expanded = ExpandNode(expansion, child, space);
                if (!expansion.Error.empty())
                    return {};
                const auto host = renumbering.ByOuter.find(child["Parent"].as<ECS::Entity>());
                YAML::Node parent = host != renumbering.ByOuter.end() ? host->second : root;
                expanded["Parent"] = parent["Entity"].as<ECS::Entity>();
                parent["Children"].push_back(expanded);
            }
            return root;
        }

        YAML::Node ExpandNode(Expansion& expansion, const YAML::Node& node, IdSpace& space)
        {
            if (node["Prefab"])
                return ExpandInstance(expansion, node, space);
            YAML::Node out(YAML::NodeType::Map);
            for (const auto& entry : node)
            {
                if (entry.first.Scalar() != "Children")
                    out[entry.first.Scalar()] = YAML::Clone(entry.second);
            }
            YAML::Node children(YAML::NodeType::Sequence);
            for (const YAML::Node& child : node["Children"])
            {
                YAML::Node expanded = ExpandNode(expansion, child, space);
                if (!expansion.Error.empty())
                    return {};
                children.push_back(expanded);
            }
            out["Children"] = children;
            return out;
        }

        std::optional<YAML::Node> Expand(Expansion& expansion, const std::string& path)
        {
            if (std::find(expansion.Chain.begin(), expansion.Chain.end(), path) != expansion.Chain.end())
            {
                std::vector<std::string> chain(std::find(expansion.Chain.begin(), expansion.Chain.end(), path),
                                               expansion.Chain.end());
                chain.push_back(path);
                expansion.Error = "prefab cycle: " + DescribePrefabChain(chain);
                return std::nullopt;
            }
            const std::shared_ptr<const YAML::Node> document = expansion.Prefabs.LoadPrefabDocument(path);
            if (!document)
            {
                expansion.Error = path + " cannot be read";
                return std::nullopt;
            }
            const YAML::Node subtree = (*document)["Subtree"];
            if (!subtree || !subtree.IsSequence() || subtree.size() != 1)
            {
                expansion.Error = path + " holds no subtree";
                return std::nullopt;
            }
            if (!HoldsInstances(subtree[0]))
                return *document;

            IdSpace space;
            space.SourceIds = (*document)["SourceIds"].as<std::vector<ECS::Entity>>();
            for (const ECS::Entity source : space.SourceIds)
                space.NextSynthetic = std::max(space.NextSynthetic, source + 1);

            expansion.Chain.push_back(path);
            YAML::Node root = ExpandNode(expansion, subtree[0], space);
            expansion.Chain.pop_back();
            if (!expansion.Error.empty())
                return std::nullopt;

            YAML::Node expanded(YAML::NodeType::Map);
            expanded["Version"] = YAML::Clone((*document)["Version"]);
            YAML::Node sources(YAML::NodeType::Sequence);
            for (const ECS::Entity source : space.SourceIds)
                sources.push_back(source);
            sources.SetStyle(YAML::EmitterStyle::Flow);
            expanded["SourceIds"] = sources;
            YAML::Node subtreeOut(YAML::NodeType::Sequence);
            subtreeOut.push_back(root);
            expanded["Subtree"] = subtreeOut;
            return expanded;
        }

        bool FindChain(IPrefabProvider& prefabs, const std::string& from, const std::string& target,
                       std::set<std::string>& visited, std::vector<std::string>& chain)
        {
            chain.push_back(from);
            if (visited.insert(from).second)
            {
                if (const std::shared_ptr<const YAML::Node> document = prefabs.LoadPrefabDocument(from))
                {
                    for (const std::string& nested : FindNestedPrefabs(*document))
                    {
                        if (nested == target)
                        {
                            chain.push_back(nested);
                            return true;
                        }
                        if (FindChain(prefabs, nested, target, visited, chain))
                            return true;
                    }
                }
            }
            chain.pop_back();
            return false;
        }
    }

    ExpandedPrefab ExpandPrefab(IPrefabProvider& prefabs, const ComponentSerializerRegistry& registry, const std::string& path)
    {
        Expansion expansion{ prefabs, registry, {}, {} };
        try
        {
            if (std::optional<YAML::Node> document = Expand(expansion, path))
                return { std::make_shared<const YAML::Node>(std::move(*document)), {} };
        }
        catch (const std::exception& e)
        {
            expansion.Error = e.what();
        }
        return { nullptr, expansion.Error.empty() ? path + " cannot be expanded" : expansion.Error };
    }

    std::vector<std::string> FindNestedPrefabs(const YAML::Node& subtreeDocument)
    {
        std::vector<std::string> nested;
        const YAML::Node         subtree = subtreeDocument["Subtree"];
        if (subtree && subtree.IsSequence())
        {
            for (const YAML::Node& node : subtree)
                CollectNested(node, nested);
        }
        return nested;
    }

    std::vector<std::string> FindPrefabChain(IPrefabProvider& prefabs, const std::string& from, const std::string& target)
    {
        std::set<std::string>    visited;
        std::vector<std::string> chain;
        if (from == target)
            return { from };
        return FindChain(prefabs, from, target, visited, chain) ? chain : std::vector<std::string>{};
    }

    std::string DescribePrefabChain(const std::vector<std::string>& chain)
    {
        std::string text;
        for (const std::string& path : chain)
            text += (text.empty() ? "" : " -> ") + path;
        return text;
    }
}
