#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

namespace Editor
{
    // What a file or folder under Assets/ is, as the Content panel shows it and later tickets act
    // on it (opening it, dragging it onto a field).
    enum class ContentType
    {
        Folder,
        Scene,
        Material,
        Texture,
        Mesh,
        Script,
        Shader,
        Pipeline,
        VertexDescription,
        RenderGraph,
        Font,
        Audio,
        Other,
    };

    // How many content types there are, for tables indexed by ContentType.
    inline constexpr size_t CONTENT_TYPE_COUNT = static_cast<size_t>(ContentType::Other) + 1;

    // The type of an entry, from its extension (case-insensitive) in one table. A .yaml file is a
    // scene only in a folder named "Scenes"; elsewhere it is Other.
    [[nodiscard]] ContentType GetContentType(std::string_view virtualPath, bool isDirectory);

    // A short label drawn in the entry's icon ("DIR", "MAT", ...).
    [[nodiscard]] const char* GetContentTypeGlyph(ContentType type);

    // The type's name, for tooltips ("Material").
    [[nodiscard]] const char* GetContentTypeName(ContentType type);

    // The type GetContentTypeName calls name, ignoring case ("mesh" is Mesh); nullopt for any
    // other name. Scripts name the asset type an AssetRef property takes this way.
    [[nodiscard]] std::optional<ContentType> FindContentTypeByName(std::string_view name);
}
