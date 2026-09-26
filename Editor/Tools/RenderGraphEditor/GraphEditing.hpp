#pragma once

#include "GraphCanvasModel.hpp"

#include "HedgehogRenderer/Graph/GraphAsset.hpp"

#include <optional>
#include <string>
#include <string_view>

// The render graph editor's edits, as plain operations on a GraphAsset. Each keeps the asset
// consistent the way the parser requires: resource names (outputs, resources and imports share one
// namespace) and pass names stay unique, bindings follow a rename, removing a resource drops the
// bindings to it, and output slots stay contiguous from 0. None of them checks what only the pass
// registry knows; the runtime's validation reports that when the graph is saved and reloaded.
namespace Editor::GraphEdit
{
    // The name, or the name with the lowest free number appended ("depth", "depth1", ...).
    [[nodiscard]] std::string UniqueResourceName(const Renderer::GraphAsset& asset, std::string_view base);
    [[nodiscard]] std::string UniquePassName(const Renderer::GraphAsset& asset, std::string_view base);

    // Add an entry under a free name derived from base, and return the name used. A new resource
    // or output is R16G16B16A16Unorm at RelativeToResult(1.0); a new import is D32Float.
    std::string AddPass(Renderer::GraphAsset& asset, std::string_view type);
    std::string AddResourceLike(Renderer::GraphAsset& asset, GraphNodeKind kind, std::string_view base);

    void RemovePass(Renderer::GraphAsset& asset, size_t index);
    void RemoveResourceLike(Renderer::GraphAsset& asset, GraphNodeKind kind, size_t index);

    // Return an error message, or nothing when the rename was applied.
    std::optional<std::string> RenamePass(Renderer::GraphAsset& asset, size_t index, std::string_view name);
    std::optional<std::string> RenameResourceLike(Renderer::GraphAsset& asset, GraphNodeKind kind, size_t index,
                                                  std::string_view name);

    // Binds the pass's slot to resource, replacing what it was bound to, or unbinds it.
    void BindSlot(Renderer::GraphAsset& asset, size_t passIndex, std::string_view slot, std::string_view resource);
    void UnbindSlot(Renderer::GraphAsset& asset, size_t passIndex, std::string_view slot);

    // Sets the pass's parameter, or removes it with no value (the pass type's default applies).
    void SetParameter(Renderer::GraphAsset& asset, size_t passIndex, std::string_view name,
                      std::optional<std::string> value);
}
