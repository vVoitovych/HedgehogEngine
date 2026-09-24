#pragma once

#include "GraphAsset.hpp"
#include "PassBuilderRegistry.hpp"

#include "RHI/api/RHITypes.hpp"

#include <string>
#include <vector>

namespace Renderer
{
    class RenderGraphRuntime;

    struct GraphInstantiationError
    {
        std::string Message;  // full human-readable description, already names pass/slot
        std::string PassName; // empty if not pass-specific
        std::string SlotName; // binding slot or output slot name; empty if neither
    };

    struct GraphInstantiationResult
    {
        bool                                 Success = false;
        std::vector<GraphInstantiationError> Errors; // populated only when !Success
    };

    // What a view requires of graph output slot i (RENDERING.md section 5.2): the view binds its
    // targets by slot index, so count, format and size policy must all match.
    struct GraphOutputRequirement
    {
        RHI::Format  Format = RHI::Format::Undefined;
        RGSizePolicy Size;
    };

    // The usage every texture an asset declares is created with: depth formats become depth
    // targets, everything else a color target, and both can be sampled. Public so a C++ caller
    // declaring the same graph by hand creates identical textures.
    [[nodiscard]] RHI::TextureUsage DefaultTextureUsage(RHI::Format format);

    // RENDERING.md section 6. Turns a parsed GraphAsset into declarations on a RenderGraphRuntime,
    // using only the runtime's public API and the registered pass builders. It has no capability
    // a C++ caller of those lacks: it creates each output and resource with CreateTexture, calls
    // each pass type's build function with a PassInvocation, and binds each output with
    // AddOutputSlot/BindOutput. The C++ equivalence oracle in RenderGraphTest holds it to that.
    //
    // Semantic validation lives here, not in the parser: registered pass types, their slots and
    // parameters, binding targets, and the view's output contract. All of it runs before anything
    // is declared, so an invalid asset leaves the runtime untouched. The one check that needs the
    // builders to have run is an output no pass writes; that slot is left unbound, so the
    // runtime's Execute() rejects the graph as well and nothing executes.
    //
    // Nothing in the frame loop calls this yet.
    class GraphInstantiator
    {
    public:
        explicit GraphInstantiator(const PassBuilderRegistry& registry) : m_Registry(registry) {}

        // requiredOutputs is the view's contract, checked when given.
        [[nodiscard]] GraphInstantiationResult Instantiate(
            const GraphAsset& asset, RenderGraphRuntime& graph,
            const std::vector<GraphOutputRequirement>* requiredOutputs = nullptr) const;

    private:
        const PassBuilderRegistry& m_Registry;
    };
}
