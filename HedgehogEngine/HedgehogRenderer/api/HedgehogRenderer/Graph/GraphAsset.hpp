#pragma once

#include "RGTypes.hpp"

#include "RHI/api/RHITypes.hpp"

#include <cstdint>
#include <string>
#include <vector>

// RENDERING.md section 6 — schema v1 of a graph asset, as GraphAssetParser produces it. Passive
// data only: every vocabulary string (formats, size policies) is already resolved to its engine
// value, but nothing here has been checked against a pass-builder registry. Which pass types exist,
// which binding slots a type accepts and what its parameters mean is instantiation's concern
// (HE-75), because a graph is meant to be editable between load and instantiation.
//
// Deliberately minimal: no expressions, no conditionals, no typed parameters. If the schema starts
// needing one, what is being expressed belongs in C++ (RENDERING.md section 6, "Schema v1").
namespace Renderer
{
    inline constexpr uint32_t GRAPH_ASSET_SCHEMA_VERSION = 1;

    // One entry of the graph's ordered output list (RENDERING.md section 5.2). Slot is the
    // index a view binds to; Name is what passes bind against inside the asset.
    struct GraphAssetOutput
    {
        uint32_t     Slot = 0;
        std::string  Name;
        RHI::Format  Format = RHI::Format::Undefined;
        RGSizePolicy Size;
    };

    // A transient resource the graph creates for itself.
    struct GraphAssetResource
    {
        std::string  Name;
        RHI::Format  Format = RHI::Format::Undefined;
        RGSizePolicy Size;
    };

    // "Slot" is a binding slot the pass type defines (e.g. "color"); "Resource" names an output
    // or a resource declared in the same asset. Neither side is resolved at parse.
    struct GraphAssetBinding
    {
        std::string Slot;
        std::string Resource;
    };

    // Parameters stay untyped strings. The pass builder that owns them interprets each one,
    // typically through the vocabulary functions in GraphAssetVocabulary.hpp.
    struct GraphAssetParameter
    {
        std::string Name;
        std::string Value;
    };

    struct GraphAssetPass
    {
        std::string                      Type;
        std::string                      Name;
        std::vector<GraphAssetBinding>   Bindings;   // document order
        std::vector<GraphAssetParameter> Parameters; // document order
    };

    struct GraphAsset
    {
        uint32_t                        Version = GRAPH_ASSET_SCHEMA_VERSION;
        std::vector<GraphAssetOutput>   Outputs;   // sorted by Slot, which runs 0..N-1
        std::vector<GraphAssetResource> Resources;
        std::vector<GraphAssetPass>     Passes;    // document order
    };
}
