#include "GraphCanvasModel.hpp"

#include "HedgehogRenderer/Graph/GraphAssetVocabulary.hpp"

#include <algorithm>
#include <unordered_map>

namespace Editor
{
    namespace
    {
        std::string FormatName(RHI::Format format)
        {
            const std::optional<std::string_view> name = Renderer::GetFormatName(format);
            return std::string(name ? *name : "Undefined");
        }

        const Renderer::GraphAssetBinding* FindBinding(const Renderer::GraphAssetPass& pass, std::string_view slot)
        {
            const auto it = std::ranges::find(pass.Bindings, slot, &Renderer::GraphAssetBinding::Slot);
            return it != pass.Bindings.end() ? &*it : nullptr;
        }

        // Where a named resource of the asset is drawn: the node and the pin links attach to.
        struct ResourceEnd
        {
            GraphNodeKind Kind = GraphNodeKind::Resource;
            uint32_t      Pin  = 0;
        };
    }

    std::string GetGraphNodeKey(GraphNodeKind kind, std::string_view name)
    {
        switch (kind)
        {
        case GraphNodeKind::Resource: return "resource:" + std::string(name);
        case GraphNodeKind::Import:   return "import:" + std::string(name);
        case GraphNodeKind::Output:   return "output:" + std::string(name);
        case GraphNodeKind::Pass:     return "pass:" + std::string(name);
        }
        return std::string(name);
    }

    GraphCanvasModel BuildGraphCanvasModel(const Renderer::GraphAsset& asset, const FindPassTypeFn& findPassType)
    {
        GraphCanvasModel model;
        uint32_t nextId = 1;
        std::unordered_map<std::string, ResourceEnd> resources;

        const auto addResourceNode = [&](GraphNodeKind kind, size_t index, const std::string& name,
                                         std::vector<std::string> details, uint32_t column, uint32_t row)
        {
            GraphCanvasNode node;
            node.Id      = nextId++;
            node.Kind    = kind;
            node.Index   = index;
            node.Name    = name;
            node.Key     = GetGraphNodeKey(kind, name);
            node.Title   = name;
            node.Details = std::move(details);
            node.Column  = column;
            node.Row     = row;

            GraphCanvasPin pin;
            pin.Id      = nextId++;
            pin.OnRight = kind != GraphNodeKind::Output; // outputs receive links from the passes
            node.Pins.push_back(pin);

            resources[name] = { kind, pin.Id };
            model.Nodes.push_back(std::move(node));
        };

        uint32_t row = 0;
        for (size_t i = 0; i < asset.Resources.size(); ++i)
        {
            const Renderer::GraphAssetResource& resource = asset.Resources[i];
            addResourceNode(GraphNodeKind::Resource, i, resource.Name,
                            { FormatName(resource.Format), Renderer::SizePolicyToString(resource.Size) }, 0, row++);
        }
        for (size_t i = 0; i < asset.Imports.size(); ++i)
        {
            const Renderer::GraphAssetImport& import = asset.Imports[i];
            addResourceNode(GraphNodeKind::Import, i, import.Name, { "import", FormatName(import.Format) }, 0, row++);
        }

        const uint32_t outputColumn = static_cast<uint32_t>(asset.Passes.size()) + 1;
        for (size_t i = 0; i < asset.Outputs.size(); ++i)
        {
            const Renderer::GraphAssetOutput& output = asset.Outputs[i];
            addResourceNode(GraphNodeKind::Output, i, output.Name,
                            { "output " + std::to_string(output.Slot), FormatName(output.Format),
                              Renderer::SizePolicyToString(output.Size) },
                            outputColumn, output.Slot);
        }

        for (size_t passIndex = 0; passIndex < asset.Passes.size(); ++passIndex)
        {
            const Renderer::GraphAssetPass& pass = asset.Passes[passIndex];
            const Renderer::PassTypeInfo*   type = findPassType ? findPassType(pass.Type) : nullptr;

            GraphCanvasNode node;
            node.Id      = nextId++;
            node.Kind    = GraphNodeKind::Pass;
            node.Index   = passIndex;
            node.Name    = pass.Name;
            node.Key     = GetGraphNodeKey(GraphNodeKind::Pass, pass.Name);
            node.Title   = pass.Name == pass.Type ? pass.Name : pass.Name + " (" + pass.Type + ")";
            node.Problem = type == nullptr;
            node.Column  = static_cast<uint32_t>(passIndex) + 1;
            if (!type)
                node.Details.push_back("unknown pass type '" + pass.Type + "'");
            for (const Renderer::GraphAssetParameter& parameter : pass.Parameters)
                node.Details.push_back(parameter.Name + " = " + parameter.Value);

            // The type's slots first, in its order, then whatever else the asset binds.
            std::vector<std::string> slots = type ? type->Slots : std::vector<std::string>{};
            for (const Renderer::GraphAssetBinding& binding : pass.Bindings)
            {
                if (std::ranges::find(slots, binding.Slot) == slots.end())
                    slots.push_back(binding.Slot);
            }

            for (const std::string& slot : slots)
            {
                GraphCanvasPin pin;
                pin.Id    = nextId++;
                pin.Slot  = slot;
                pin.Label = slot;

                const bool declared = !type || std::ranges::find(type->Slots, slot) != type->Slots.end();
                const Renderer::GraphAssetBinding* binding = FindBinding(pass, slot);
                const auto resource = binding ? resources.find(binding->Resource) : resources.end();

                if (!declared)
                {
                    pin.Label  += " (not a slot of " + pass.Type + ")";
                    pin.Problem = true;
                }
                if (!binding)
                {
                    pin.Label  += " (unbound)";
                    pin.Problem = true;
                }
                else if (resource == resources.end())
                {
                    pin.Label  += " -> '" + binding->Resource + "' (not declared)";
                    pin.Problem = true;
                }
                else if (resource->second.Kind == GraphNodeKind::Output)
                {
                    pin.OnRight = true;
                    model.Links.push_back({ 0, pin.Id, resource->second.Pin, passIndex, slot });
                }
                else
                {
                    model.Links.push_back({ 0, resource->second.Pin, pin.Id, passIndex, slot });
                }
                node.Pins.push_back(std::move(pin));
            }
            model.Nodes.push_back(std::move(node));
        }

        for (GraphCanvasLink& link : model.Links)
            link.Id = nextId++;
        return model;
    }
}
