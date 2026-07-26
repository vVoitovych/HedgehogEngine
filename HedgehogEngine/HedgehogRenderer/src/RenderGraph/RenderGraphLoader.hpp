#pragma once

#include "RenderGraphTypes.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace FS
{
    class FileSystemManager;
}

namespace Renderer
{
    struct GraphAssetDesc;
    class RenderPassRegistry;

    // Loads and validates .rgq pipeline-composition assets — see workflow/current-plan.md,
    // ".rgq schema (version 1)" for the authoritative schema and validation-rule (V1-V8) text.
    class RenderGraphLoader
    {
    public:
        // Pure function over text — no filesystem, no device — exactly like
        // EcsSerializer::DeserializeFromString, so the full V1-V8 validation surface is
        // unit-testable without a Vulkan device (see the RenderGraphTest suite). sourceName is
        // used only in log messages (a virtual path, or e.g. "<string>" for an in-memory test
        // case). expectedStage and passRegistry are two deliberate additions beyond the plan's
        // original two-argument sketch — V2 ("the requested load slot matches stage") and V4's
        // "unregistered type" half both need this information to be checkable as a pure function,
        // which the plan's own test requirements call for (see workflow/current-plan.md, Phase 5
        // completion note).
        [[nodiscard]] static std::optional<GraphAssetDesc> Parse(std::string_view text,
                                                                  std::string_view sourceName,
                                                                  GraphStage expectedStage,
                                                                  const RenderPassRegistry& passRegistry);

        [[nodiscard]] static std::optional<GraphAssetDesc> Load(const std::string& virtualPath,
                                                                 GraphStage expectedStage,
                                                                 const RenderPassRegistry& passRegistry,
                                                                 const FS::FileSystemManager& fileSystem);
    };
}
