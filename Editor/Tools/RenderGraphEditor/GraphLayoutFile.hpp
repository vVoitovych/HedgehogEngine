#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace Editor
{
    struct GraphNodePosition
    {
        float X = 0.0f;
        float Y = 0.0f;

        bool operator==(const GraphNodePosition& other) const = default;
    };

    // Where the render graph editor last left each node of a graph, by node key ("pass:Forward").
    // Kept beside the graph as "<name>.graph.layout", so the .graph file stays free of editor data
    // (GraphAssetParser rejects unknown keys) and a layout change never touches the graph itself.
    using GraphLayout = std::map<std::string, GraphNodePosition>;

    [[nodiscard]] std::filesystem::path GetGraphLayoutFile(const std::filesystem::path& graphFile);

    // Nothing if the file does not exist. A file that cannot be read or parsed logs a warning and
    // also gives nothing, so the editor falls back to the automatic layout.
    [[nodiscard]] std::optional<GraphLayout> LoadGraphLayout(const std::filesystem::path& layoutFile);

    // Logs a warning and returns false if the file cannot be written.
    bool SaveGraphLayout(const std::filesystem::path& layoutFile, const GraphLayout& layout);
}
