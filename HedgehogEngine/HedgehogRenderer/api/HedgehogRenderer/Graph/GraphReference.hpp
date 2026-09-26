#pragma once

#include <string>
#include <string_view>

// How a view (a CameraComponent's GraphName) names the graph it renders with. A graph reference is
// either the name of a graph the renderer registered from its own graph folder ("game", "scene"),
// or the path of a .graph file anywhere: a virtual path ("assets://Graphs/bloom.graph") resolved
// through the FileSystem, or an absolute path ("D:/Graphs/bloom.graph").
namespace Renderer
{
    enum class GraphReferenceKind
    {
        Name,
        File,
    };

    // A file when the reference contains "://", '/' or '\', or ends in ".graph"; a name otherwise.
    [[nodiscard]] GraphReferenceKind ClassifyGraphReference(std::string_view reference);

    // True for a virtual path, one naming a FileSystem mount ("assets://...").
    [[nodiscard]] bool IsVirtualGraphPath(std::string_view reference);

    // The key a reference is registered and found under. A name is returned as it is. A file path
    // gets '/' separators; an absolute one is also lexically normalized ("a/./b/../c" is "a/c")
    // and its drive letter lower-cased, so different spellings of one file register once.
    [[nodiscard]] std::string NormalizeGraphReference(std::string_view reference);
}
