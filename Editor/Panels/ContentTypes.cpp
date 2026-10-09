#include "ContentTypes.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace Editor
{
    namespace
    {
        struct ExtensionType
        {
            std::string_view Extension;
            ContentType      Type;
        };

        // Every extension the Content panel recognises. .yaml is handled apart: it depends on the folder.
        constexpr std::array EXTENSION_TYPES = {
            ExtensionType{ ".material", ContentType::Material },
            ExtensionType{ ".png",      ContentType::Texture },
            ExtensionType{ ".jpg",      ContentType::Texture },
            ExtensionType{ ".jpeg",     ContentType::Texture },
            ExtensionType{ ".tga",      ContentType::Texture },
            ExtensionType{ ".obj",      ContentType::Mesh },
            ExtensionType{ ".gltf",     ContentType::Mesh },
            ExtensionType{ ".glb",      ContentType::Mesh },
            ExtensionType{ ".lua",      ContentType::Script },
            ExtensionType{ ".shader",   ContentType::Shader },
            ExtensionType{ ".vert",     ContentType::Shader },
            ExtensionType{ ".frag",     ContentType::Shader },
            ExtensionType{ ".comp",     ContentType::Shader },
            ExtensionType{ ".glsl",     ContentType::Shader },
            ExtensionType{ ".pl",       ContentType::Pipeline },
            ExtensionType{ ".vdes",     ContentType::VertexDescription },
            ExtensionType{ ".graph",    ContentType::RenderGraph },
            ExtensionType{ ".ttf",      ContentType::Font },
            ExtensionType{ ".otf",      ContentType::Font },
            ExtensionType{ ".wav",      ContentType::Audio },
            ExtensionType{ ".mp3",      ContentType::Audio },
            ExtensionType{ ".flac",     ContentType::Audio },
            ExtensionType{ ".prefab",   ContentType::Prefab },
            ExtensionType{ ".hdr",      ContentType::Environment },
        };

        constexpr std::string_view SCENE_EXTENSION = ".yaml";
        constexpr std::string_view SCENE_FOLDER    = "Scenes";

        std::string ToLower(std::string_view text)
        {
            std::string lower(text);
            std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return lower;
        }

        // The name of the folder holding the entry: "Scenes" for "assets://Scenes/a.yaml".
        std::string_view ParentFolderName(std::string_view path)
        {
            const size_t slash = path.find_last_of('/');
            if (slash == std::string_view::npos)
                return {};
            path = path.substr(0, slash);
            const size_t previous = path.find_last_of('/');
            return previous == std::string_view::npos ? path : path.substr(previous + 1);
        }
    }

    ContentType GetContentType(std::string_view virtualPath, bool isDirectory)
    {
        if (isDirectory)
            return ContentType::Folder;

        const size_t slash = virtualPath.find_last_of('/');
        const size_t dot   = virtualPath.find_last_of('.');
        if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash))
            return ContentType::Other;

        const std::string extension = ToLower(virtualPath.substr(dot));
        if (extension == SCENE_EXTENSION)
            return ParentFolderName(virtualPath) == SCENE_FOLDER ? ContentType::Scene : ContentType::Other;

        const auto it = std::ranges::find(EXTENSION_TYPES, std::string_view(extension), &ExtensionType::Extension);
        return it != EXTENSION_TYPES.end() ? it->Type : ContentType::Other;
    }

    const char* GetContentTypeGlyph(ContentType type)
    {
        switch (type)
        {
        case ContentType::Folder:            return "DIR";
        case ContentType::Scene:             return "SCN";
        case ContentType::Material:          return "MAT";
        case ContentType::Texture:           return "TEX";
        case ContentType::Mesh:              return "MSH";
        case ContentType::Script:            return "LUA";
        case ContentType::Shader:            return "SHD";
        case ContentType::Pipeline:          return "PL";
        case ContentType::VertexDescription: return "VD";
        case ContentType::RenderGraph:       return "RG";
        case ContentType::Font:              return "FNT";
        case ContentType::Audio:             return "AUD";
        case ContentType::Prefab:            return "PFB";
        case ContentType::Environment:       return "HDR";
        case ContentType::Other:             return "?";
        }
        return "?";
    }

    const char* GetContentTypeName(ContentType type)
    {
        switch (type)
        {
        case ContentType::Folder:            return "Folder";
        case ContentType::Scene:             return "Scene";
        case ContentType::Material:          return "Material";
        case ContentType::Texture:           return "Texture";
        case ContentType::Mesh:              return "Mesh";
        case ContentType::Script:            return "Script";
        case ContentType::Shader:            return "Shader";
        case ContentType::Pipeline:          return "Pipeline";
        case ContentType::VertexDescription: return "Vertex description";
        case ContentType::RenderGraph:       return "Render graph";
        case ContentType::Font:              return "Font";
        case ContentType::Audio:             return "Audio";
        case ContentType::Prefab:            return "Prefab";
        case ContentType::Environment:       return "Environment";
        case ContentType::Other:             return "File";
        }
        return "File";
    }

    std::optional<ContentType> FindContentTypeByName(std::string_view name)
    {
        const std::string wanted = ToLower(name);
        for (size_t index = 0; index < CONTENT_TYPE_COUNT; ++index)
        {
            const auto type = static_cast<ContentType>(index);
            if (ToLower(GetContentTypeName(type)) == wanted)
                return type;
        }
        return std::nullopt;
    }
}
