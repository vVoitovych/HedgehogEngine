#pragma once

#include "RenderGraphTypes.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Renderer
{
    // A declared transient texture — the parsed form of one entry in a .rgq file's `resources:`
    // list. Kept as plain vocabulary strings (not resolved RHI::Format/TextureUsage) so the loader
    // that produces this struct stays device-free — see RenderGraphVocabulary.hpp.
    struct ResourceDesc
    {
        std::string Name;
        std::string Format;                    // e.g. "rgba16", "depth_preferred"
        std::string Size;                      // "view" | "swapchain" | "fixed"
        uint32_t    FixedWidth  = 0;            // only meaningful when Size == "fixed"
        uint32_t    FixedHeight = 0;
        std::vector<std::string> UsageFlags;    // e.g. {"color_attachment", "sampled"}
    };

    // One input/output reference on a node. Name follows the schema's reference syntax: a bare
    // name resolves within this file's own `resources:`; "shared:x" / "views:x" / "views:*" import
    // from another graph's scope — see RenderGraphLoader's "Name reference syntax" validation (V6).
    struct SlotDesc
    {
        std::string Name;
        std::string Usage;   // e.g. "colorWrite", "depthRead" — see RenderGraphVocabulary
    };

    struct NodeDesc
    {
        std::string Type;       // must be registered in RenderPassRegistry
        std::string Instance;   // unique within the file
        bool        Enabled = true;
        std::vector<SlotDesc> Inputs;
        std::vector<SlotDesc> Outputs;
        std::unordered_map<std::string, std::string> Params;   // reserved for Phase 6+; unused today
    };

    // The parsed, device-free description of a .rgq file — RenderGraphLoader::Parse's output.
    // Resource declarations are validated (V5) but, per this session's Phase 5 design decision,
    // NOT mechanically wired to graph texture creation: each pass still declares its own transient
    // textures in Setup() (format/usage stays co-located with the shader/pipeline that needs it —
    // see workflow/current-plan.md, Phase 5 completion note). `Resources` therefore documents and
    // validates a pipeline's declared surface without being a second source of truth for it.
    struct GraphAssetDesc
    {
        int                       Version = 0;
        GraphStage                Stage   = GraphStage::View;
        std::vector<ResourceDesc> Resources;
        std::vector<NodeDesc>     Nodes;
    };
}
