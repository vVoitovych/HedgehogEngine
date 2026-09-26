#pragma once

#include "GraphAsset.hpp"

#include <filesystem>
#include <string>

namespace Renderer
{
    // The reverse of GraphAssetParser: a GraphAsset as .graph YAML (RENDERING.md section 6), for the
    // render graph editor to save with. GraphAssetParser::Parse of the text gives back an equal
    // asset, and writing an unchanged asset again gives the same text.
    //
    // The layout is fixed so a small edit gives a small diff: version, outputs, resources, imports,
    // passes, in that order; each entry's keys in the parser's documented order; bindings and
    // parameters in their document order. Empty sections are left out, as the parser allows.
    // Every format and size policy is spelled by GraphAssetVocabulary.
    //
    // Comments are not part of a GraphAsset, so a hand-written file saved through the writer loses
    // them. The asset is written as it is, without validation, so an asset the schema does not
    // allow is written anyway and rejected when the file is read back: an Undefined format is
    // written as "Undefined", and an empty name or parameter value as "".
    //
    // Device-free. WriteGraphAsset does no IO; WriteGraphAssetFile does no logging.
    [[nodiscard]] std::string WriteGraphAsset(const GraphAsset& asset);

    // Writes the asset to a temporary file next to file, then renames it over file, so a reader
    // polling for changes never sees a half-written graph. Returns false and sets *error (if not
    // null) when either step fails, leaving any existing file as it was.
    [[nodiscard]] bool WriteGraphAssetFile(const GraphAsset& asset, const std::filesystem::path& file,
                                           std::string* error = nullptr);
}
