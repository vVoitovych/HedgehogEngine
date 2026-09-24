#pragma once

#include "GraphAsset.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace Renderer
{
    struct GraphAssetError
    {
        std::string Message; // full human-readable description, already names the offender
        std::string Path;    // where in the document, e.g. "passes[2]" or "outputs[0].format"
    };

    struct GraphAssetParseResult
    {
        bool                         Success = false;
        std::vector<GraphAssetError> Errors; // populated only when !Success
        GraphAsset                   Asset;  // populated only when Success
    };

    // RENDERING.md section 6. Turns a graph asset's YAML text into a GraphAsset, checking that the
    // document is well-formed: a supported schema version, the required keys, no unknown keys,
    // every vocabulary string resolvable, slots contiguous from 0, and no duplicate names.
    //
    // It does not check anything that needs the pass-builder registry: unknown pass types,
    // bindings to slots a pass type lacks, bindings that name no declared resource, or what a
    // parameter means. Instantiation (HE-75) checks those, because a graph is meant to be
    // editable between load and instantiation.
    //
    // Device-free and side-effect free: no file IO, no logging. The caller that loads a file owns
    // both. Like GraphCompiler, it never succeeds partially: either Success is true and Asset is
    // filled in, or Errors lists every problem found. The one exception is an unsupported schema
    // version, which stops parsing, since nothing else about the document can be interpreted.
    class GraphAssetParser
    {
    public:
        [[nodiscard]] GraphAssetParseResult Parse(std::string_view yamlText) const;
    };
}
