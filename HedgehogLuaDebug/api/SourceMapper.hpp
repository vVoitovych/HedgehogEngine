#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

namespace LuaDebug
{
    // Turns a Lua chunk name into the file path breakpoints and stack frames use. Scripts load
    // with their virtual path ("@assets://Scripts/Player.lua"), resolved through the resolver
    // (the engine's FS::FileSystemManager); a chunk named by a physical path is used as it is.
    // Paths are normalized (absolute, folded, forward slashes, lower case: Windows paths ignore
    // case), so the client's spelling of a file and the engine's meet.
    class SourceMapper
    {
    public:
        using Resolver = std::function<std::optional<std::filesystem::path>(const std::string& virtualPath)>;

        explicit SourceMapper(Resolver resolver = {});

        // The normalized path of a chunk, or empty for one not loaded from a file ("=stdin", a
        // string chunk) or a virtual path the resolver does not know. Cached by chunk name.
        [[nodiscard]] const std::string& ToPath(const char* chunkName);

        [[nodiscard]] static std::string NormalizePath(const std::string& physicalPath);

    private:
        Resolver                                     m_Resolver;
        std::unordered_map<std::string, std::string> m_Cache;
    };
}
