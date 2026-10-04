#include "HedgehogLuaDebug/api/SourceMapper.hpp"

#include <algorithm>
#include <cctype>

namespace LuaDebug
{
    namespace
    {
        constexpr std::string_view MOUNT_SEPARATOR = "://";
    }

    SourceMapper::SourceMapper(Resolver resolver)
        : m_Resolver(std::move(resolver))
    {
    }

    std::string SourceMapper::NormalizePath(const std::string& physicalPath)
    {
        if (physicalPath.empty())
            return {};
        std::error_code       error;
        std::filesystem::path path = std::filesystem::absolute(std::filesystem::path(physicalPath), error);
        if (error)
            path = physicalPath;
        std::string normalized = path.lexically_normal().generic_string();
        std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return normalized;
    }

    const std::string& SourceMapper::ToPath(const char* chunkName)
    {
        const std::string name = chunkName ? chunkName : "";
        if (const auto it = m_Cache.find(name); it != m_Cache.end())
            return it->second;

        std::string path;
        // Only "@" chunks come from files; "=" and string chunks have no file.
        if (name.size() > 1 && name.front() == '@')
        {
            const std::string file = name.substr(1);
            if (file.find(MOUNT_SEPARATOR) == std::string::npos)
                path = NormalizePath(file);
            else if (m_Resolver)
                if (const std::optional<std::filesystem::path> physical = m_Resolver(file))
                    path = NormalizePath(physical->string());
        }
        return m_Cache.emplace(name, std::move(path)).first->second;
    }
}
