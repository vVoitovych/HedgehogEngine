#include "RenderGraphLoader.hpp"

#include "RenderGraphDesc.hpp"
#include "RenderGraphVocabulary.hpp"
#include "RenderPassRegistry.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "Logger/api/Logger.hpp"

#include <yaml-cpp/yaml.h>

#include <unordered_set>

namespace Renderer
{
namespace
{
    std::optional<GraphStage> ParseStage(const std::string& s)
    {
        if (s == "frame")       return GraphStage::Frame;
        if (s == "view")        return GraphStage::View;
        if (s == "composition") return GraphStage::Composition;
        return std::nullopt;
    }

    const char* StageName(GraphStage stage)
    {
        switch (stage)
        {
            case GraphStage::Frame:       return "frame";
            case GraphStage::View:        return "view";
            case GraphStage::Composition: return "composition";
        }
        return "?";
    }
}

    std::optional<GraphAssetDesc> RenderGraphLoader::Parse(std::string_view text, std::string_view sourceName,
                                                            GraphStage expectedStage,
                                                            const RenderPassRegistry& passRegistry)
    {
        try
        {
            const YAML::Node root = YAML::Load(std::string(text));

            GraphAssetDesc desc;

            // V1
            if (!root["version"])
            {
                LOGERROR("RenderGraphLoader: '", sourceName, "': missing 'version'.");
                return std::nullopt;
            }
            const int version = root["version"].as<int>();
            if (version != 1)
            {
                LOGERROR("RenderGraphLoader: '", sourceName, "': unsupported version ", version, " (expected 1).");
                return std::nullopt;
            }
            desc.Version = version;

            if (!root["stage"])
            {
                LOGERROR("RenderGraphLoader: '", sourceName, "': missing 'stage'.");
                return std::nullopt;
            }
            const std::string stageStr = root["stage"].as<std::string>();
            const auto        stage    = ParseStage(stageStr);
            if (!stage)
            {
                LOGERROR("RenderGraphLoader: '", sourceName, "': unknown stage '", stageStr,
                         "' (expected frame|view|composition).");
                return std::nullopt;
            }
            desc.Stage = *stage;

            // V2
            if (desc.Stage != expectedStage)
            {
                LOGERROR("RenderGraphLoader: '", sourceName, "': stage '", stageStr,
                         "' does not match the requested load slot '", StageName(expectedStage), "'.");
                return std::nullopt;
            }

            // resources (optional)
            std::unordered_set<std::string> localResourceNames;
            if (const YAML::Node& resourcesNode = root["resources"])
            {
                for (const YAML::Node& r : resourcesNode)
                {
                    if (!r["name"])
                    {
                        LOGERROR("RenderGraphLoader: '", sourceName, "': a resource is missing 'name'.");
                        return std::nullopt;
                    }

                    ResourceDesc rd;
                    rd.Name = r["name"].as<std::string>();

                    if (!localResourceNames.insert(rd.Name).second)
                    {
                        LOGERROR("RenderGraphLoader: '", sourceName, "': duplicate resource name '", rd.Name, "'.");
                        return std::nullopt;
                    }

                    // V5: kind
                    if (!r["kind"] || r["kind"].as<std::string>() != "texture")
                    {
                        LOGERROR("RenderGraphLoader: '", sourceName, "': resource '", rd.Name,
                                 "' has an unknown or missing 'kind' (only 'texture' is supported in v1).");
                        return std::nullopt;
                    }

                    // V5: format
                    if (!r["format"])
                    {
                        LOGERROR("RenderGraphLoader: '", sourceName, "': resource '", rd.Name, "' is missing 'format'.");
                        return std::nullopt;
                    }
                    rd.Format = r["format"].as<std::string>();
                    if (!RenderGraphVocabulary::IsKnownFormat(rd.Format))
                    {
                        LOGERROR("RenderGraphLoader: '", sourceName, "': resource '", rd.Name,
                                 "' has unknown format '", rd.Format, "'.");
                        return std::nullopt;
                    }

                    // V5: size
                    if (!r["size"])
                    {
                        LOGERROR("RenderGraphLoader: '", sourceName, "': resource '", rd.Name, "' is missing 'size'.");
                        return std::nullopt;
                    }
                    rd.Size = r["size"].as<std::string>();
                    if (!RenderGraphVocabulary::IsKnownSizeClass(rd.Size))
                    {
                        LOGERROR("RenderGraphLoader: '", sourceName, "': resource '", rd.Name,
                                 "' has unknown size '", rd.Size, "'.");
                        return std::nullopt;
                    }
                    if (rd.Size == "fixed")
                    {
                        const bool hasWidth  = r["width"]  && r["width"].as<int>()  > 0;
                        const bool hasHeight = r["height"] && r["height"].as<int>() > 0;
                        if (!hasWidth || !hasHeight)
                        {
                            LOGERROR("RenderGraphLoader: '", sourceName, "': resource '", rd.Name,
                                     "' has size: fixed but no positive width/height.");
                            return std::nullopt;
                        }
                        rd.FixedWidth  = r["width"].as<uint32_t>();
                        rd.FixedHeight = r["height"].as<uint32_t>();
                    }
                    if (rd.Size == "view" && desc.Stage != GraphStage::View)
                    {
                        LOGERROR("RenderGraphLoader: '", sourceName, "': resource '", rd.Name,
                                 "' declares size: view outside stage: view.");
                        return std::nullopt;
                    }

                    // V5: usage flags
                    if (const YAML::Node& usageNode = r["usage"])
                    {
                        for (const YAML::Node& u : usageNode)
                        {
                            const std::string flag = u.as<std::string>();
                            if (!RenderGraphVocabulary::IsKnownUsageFlag(flag))
                            {
                                LOGERROR("RenderGraphLoader: '", sourceName, "': resource '", rd.Name,
                                         "' has unknown usage flag '", flag, "'.");
                                return std::nullopt;
                            }
                            rd.UsageFlags.push_back(flag);
                        }
                    }

                    desc.Resources.push_back(std::move(rd));
                }
            }

            // V3: nodes present, non-empty
            const YAML::Node& nodesNode = root["nodes"];
            if (!nodesNode || nodesNode.size() == 0)
            {
                LOGERROR("RenderGraphLoader: '", sourceName, "': 'nodes' is missing or empty.");
                return std::nullopt;
            }

            std::unordered_set<std::string> instanceNames;
            std::unordered_set<std::string> referencedLocal; // V8
            std::unordered_set<std::string> writtenLocal;    // V8
            int initPassCount    = 0;
            int presentPassCount = 0;

            auto validateSlot = [&](const std::string& nodeInstance, const char* slotKind,
                                    const YAML::Node& slotNode, bool isWrite) -> std::optional<SlotDesc>
            {
                if (!slotNode["name"] || !slotNode["usage"])
                {
                    LOGERROR("RenderGraphLoader: '", sourceName, "': node '", nodeInstance, "' has a malformed ",
                             slotKind, " entry (needs 'name' and 'usage').");
                    return std::nullopt;
                }

                SlotDesc slot;
                slot.Name  = slotNode["name"].as<std::string>();
                slot.Usage = slotNode["usage"].as<std::string>();

                // V7
                if (!RenderGraphVocabulary::IsKnownSlotUsage(slot.Usage))
                {
                    LOGERROR("RenderGraphLoader: '", sourceName, "': node '", nodeInstance, "' ", slotKind,
                             " '", slot.Name, "' has unknown usage '", slot.Usage, "'.");
                    return std::nullopt;
                }

                // V6: name resolution
                if (slot.Name.rfind("shared:", 0) == 0)
                {
                    if (slot.Name.size() <= 7)
                    {
                        LOGERROR("RenderGraphLoader: '", sourceName, "': node '", nodeInstance,
                                 "' has a malformed 'shared:' reference '", slot.Name, "'.");
                        return std::nullopt;
                    }
                }
                else if (slot.Name.rfind("views:", 0) == 0)
                {
                    if (slot.Name.size() <= 6)
                    {
                        LOGERROR("RenderGraphLoader: '", sourceName, "': node '", nodeInstance,
                                 "' has a malformed 'views:' reference '", slot.Name, "'.");
                        return std::nullopt;
                    }
                }
                else
                {
                    if (!localResourceNames.count(slot.Name))
                    {
                        LOGERROR("RenderGraphLoader: '", sourceName, "': node '", nodeInstance, "' ", slotKind,
                                 " references undeclared resource '", slot.Name, "'.");
                        return std::nullopt;
                    }
                    referencedLocal.insert(slot.Name);
                    if (isWrite)
                        writtenLocal.insert(slot.Name);
                }

                return slot;
            };

            for (const YAML::Node& n : nodesNode)
            {
                if (!n["type"] || !n["instance"])
                {
                    LOGERROR("RenderGraphLoader: '", sourceName, "': a node is missing 'type' or 'instance'.");
                    return std::nullopt;
                }

                NodeDesc node;
                node.Type     = n["type"].as<std::string>();
                node.Instance = n["instance"].as<std::string>();
                node.Enabled  = n["enabled"] ? n["enabled"].as<bool>() : true;

                // V4: duplicate instance
                if (!instanceNames.insert(node.Instance).second)
                {
                    LOGERROR("RenderGraphLoader: '", sourceName, "': duplicate node instance '", node.Instance, "'.");
                    return std::nullopt;
                }

                // V4: registered type
                if (!passRegistry.IsRegistered(node.Type))
                {
                    LOGERROR("RenderGraphLoader: '", sourceName, "': node '", node.Instance,
                             "' has unregistered type '", node.Type, "'.");
                    return std::nullopt;
                }

                if (const YAML::Node& inputsNode = n["inputs"])
                {
                    for (const YAML::Node& in : inputsNode)
                    {
                        auto slot = validateSlot(node.Instance, "input", in, false);
                        if (!slot)
                            return std::nullopt;
                        node.Inputs.push_back(std::move(*slot));
                    }
                }

                if (const YAML::Node& outputsNode = n["outputs"])
                {
                    for (const YAML::Node& out : outputsNode)
                    {
                        auto slot = validateSlot(node.Instance, "output", out, true);
                        if (!slot)
                            return std::nullopt;
                        node.Outputs.push_back(std::move(*slot));
                    }
                }

                // Only enabled nodes count toward V3 — a disabled InitPass/PresentPass would still
                // leave the command buffer never begun/ended, which is exactly what V3 guards against.
                if (node.Enabled && node.Type == "InitPass")    ++initPassCount;
                if (node.Enabled && node.Type == "PresentPass") ++presentPassCount;

                // V3 refinement: PresentPass's Setup() is genuinely data-driven by its single
                // "presentSource" input (see PresentPass.cpp) — a node with none or several would
                // construct successfully but assert at graph-compile time, defeating the loader's
                // whole point of catching bad content before it reaches the device.
                if (node.Type == "PresentPass" && node.Inputs.size() != 1)
                {
                    LOGERROR("RenderGraphLoader: '", sourceName, "': PresentPass node '", node.Instance,
                             "' must declare exactly one input (found ", node.Inputs.size(), ").");
                    return std::nullopt;
                }

                desc.Nodes.push_back(std::move(node));
            }

            // V3: exactly-one InitPass/PresentPass in frame/composition assets
            if (desc.Stage == GraphStage::Frame && initPassCount != 1)
            {
                LOGERROR("RenderGraphLoader: '", sourceName,
                         "': a 'frame' asset must contain exactly one enabled InitPass node (found ",
                         initPassCount, ").");
                return std::nullopt;
            }
            if (desc.Stage == GraphStage::Composition && presentPassCount != 1)
            {
                LOGERROR("RenderGraphLoader: '", sourceName,
                         "': a 'composition' asset must contain exactly one enabled PresentPass node (found ",
                         presentPassCount, ").");
                return std::nullopt;
            }

            // V8: warnings, not errors — local resources only (shared:/views: imports are legitimately
            // produced by another file, unknowable from this one).
            for (const auto& r : desc.Resources)
            {
                if (!referencedLocal.count(r.Name))
                    LOGWARNING("RenderGraphLoader: '", sourceName, "': resource '", r.Name,
                              "' is declared but never referenced by any node.");
                else if (!writtenLocal.count(r.Name))
                    LOGWARNING("RenderGraphLoader: '", sourceName, "': resource '", r.Name,
                              "' is read but never written by any node in this file.");
            }

            return desc;
        }
        catch (const YAML::Exception& e)
        {
            LOGERROR("RenderGraphLoader: '", sourceName, "': malformed content: ", e.what());
            return std::nullopt;
        }
    }

    std::optional<GraphAssetDesc> RenderGraphLoader::Load(const std::string& virtualPath, GraphStage expectedStage,
                                                           const RenderPassRegistry& passRegistry,
                                                           const FS::FileSystemManager& fileSystem)
    {
        const auto text = fileSystem.ReadTextFile(virtualPath);
        if (!text)
        {
            LOGERROR("RenderGraphLoader: failed to read '", virtualPath, "'.");
            return std::nullopt;
        }

        return Parse(*text, virtualPath, expectedStage, passRegistry);
    }
}
