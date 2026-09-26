#include "HedgehogRenderer/Graph/GraphReference.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace Renderer
{
    namespace
    {
        constexpr std::string_view VIRTUAL_SEPARATOR = "://";
        constexpr std::string_view GRAPH_EXTENSION   = ".graph";
    }

    GraphReferenceKind ClassifyGraphReference(std::string_view reference)
    {
        const bool isPath = reference.find(VIRTUAL_SEPARATOR) != std::string_view::npos
                         || reference.find_first_of("/\\") != std::string_view::npos
                         || reference.ends_with(GRAPH_EXTENSION);
        return isPath ? GraphReferenceKind::File : GraphReferenceKind::Name;
    }

    bool IsVirtualGraphPath(std::string_view reference)
    {
        return reference.find(VIRTUAL_SEPARATOR) != std::string_view::npos;
    }

    std::string NormalizeGraphReference(std::string_view reference)
    {
        if (ClassifyGraphReference(reference) == GraphReferenceKind::Name)
            return std::string(reference);

        std::string path(reference);
        std::ranges::replace(path, '\\', '/');
        if (IsVirtualGraphPath(path))
            return path; // "assets://a/b": lexical normalization would fold the "//" of the mount

        path = std::filesystem::path(path).lexically_normal().generic_string();
        if (path.size() >= 2 && path[1] == ':' && std::isalpha(static_cast<unsigned char>(path[0])))
            path[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(path[0])));
        return path;
    }
}
