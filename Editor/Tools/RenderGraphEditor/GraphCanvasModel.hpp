#pragma once

#include "HedgehogRenderer/Graph/GraphAsset.hpp"
#include "HedgehogRenderer/Graph/PassBuilderRegistry.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace Editor
{
    enum class GraphNodeKind
    {
        Resource, // a transient the graph creates
        Import,   // a resource handed in from outside, e.g. the shared shadow atlas
        Output,   // one of the graph's ordered outputs
        Pass,
    };

    // A pin sits on the node's left (it reads a resource or import into a slot) or its right (the
    // slot is bound to one of the graph's outputs, which the pass produces).
    struct GraphCanvasPin
    {
        uint32_t    Id = 0;
        std::string Label;
        bool        OnRight = false;
        bool        Problem = false; // an unbound slot, a binding to nothing, a slot the type lacks
    };

    struct GraphCanvasNode
    {
        uint32_t                    Id = 0;
        GraphNodeKind               Kind = GraphNodeKind::Pass;
        std::string                 Key;     // "pass:Forward": names the node in the layout file
        std::string                 Title;
        std::vector<std::string>    Details; // format, size, "cullBackFaces = true", ...
        std::vector<GraphCanvasPin> Pins;
        bool                        Problem = false; // a pass type the registry does not know

        // The automatic layout: resources and imports in column 0, each pass in its own column in
        // file order, outputs in the last column.
        uint32_t Column = 0;
        uint32_t Row    = 0;
    };

    struct GraphCanvasLink
    {
        uint32_t Id      = 0;
        uint32_t FromPin = 0;
        uint32_t ToPin   = 0;
    };

    struct GraphCanvasModel
    {
        std::vector<GraphCanvasNode> Nodes;
        std::vector<GraphCanvasLink> Links;
    };

    using FindPassTypeFn = std::function<const Renderer::PassTypeInfo*(std::string_view type)>;

    // What the render graph editor draws for asset. A pass's pins are its type's slots in the
    // registry's order, then any slot the asset binds that the type does not declare; a pass whose
    // type is not registered gets one pin per binding. Every binding to a declared name becomes a
    // link. Ids are unique across nodes, pins and links, and depend only on asset and the registry.
    [[nodiscard]] GraphCanvasModel BuildGraphCanvasModel(const Renderer::GraphAsset& asset,
                                                         const FindPassTypeFn& findPassType);
}
