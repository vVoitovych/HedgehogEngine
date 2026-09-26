#pragma once

#include "GraphAsset.hpp"
#include "PassBuilderRegistry.hpp"

#include "RHI/api/IRHIDevice.hpp"

#include <string>
#include <vector>

namespace Renderer
{
    // One reason a graph asset cannot render, tagged with what it concerns so a tool can point at
    // it. Each tag is empty when the problem is not about that kind of thing.
    struct GraphDiagnostic
    {
        std::string Message;      // full human-readable description, already names the offender
        std::string PassName;     // the pass it concerns
        std::string SlotName;     // the pass's binding slot, or an output slot's name
        std::string ResourceName; // the output, resource or import it concerns
    };

    // Everything that stands between a parsed graph asset and a frame, checked without a GPU:
    // GraphInstantiator's semantic validation (pass types, slots, bindings, parameters), then the
    // graph is declared into a scratch RenderGraphRuntime and compiled, which finds what only the
    // whole graph shows (a resource read before any pass writes it, a cycle). Declaring never
    // touches the device, so any IRHIDevice works; the scratch runtime is discarded.
    //
    // Empty means the graph would render. Imports are declared as external textures, as the shared
    // frame phase supplies them. What a particular view requires of the graph's outputs is not
    // checked: that depends on the view, not the asset.
    [[nodiscard]] std::vector<GraphDiagnostic> DiagnoseGraphAsset(const GraphAsset& asset,
                                                                  const PassBuilderRegistry& registry,
                                                                  RHI::IRHIDevice& device);
}
