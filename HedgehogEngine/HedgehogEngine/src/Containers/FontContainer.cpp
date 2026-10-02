#include "HedgehogEngine/api/Containers/FontContainer.hpp"

#include "ContentLoader/api/FontLoader.hpp"

#include <algorithm>
#include <string_view>

namespace HedgehogEngine
{
    namespace
    {
        constexpr std::string_view ASSETS_PREFIX = "assets://";

        // A path under assets:// without the prefix, with forward slashes.
        std::string NormalizeFontPath(const std::string& path)
        {
            std::string normalized = path.starts_with(ASSETS_PREFIX) ? path.substr(ASSETS_PREFIX.size()) : path;
            std::replace(normalized.begin(), normalized.end(), '\\', '/');
            return normalized;
        }
    }

    FontContainer::FontContainer(const FS::FileSystemManager& fileSystem)
        : m_FileSystem(fileSystem)
    {
    }

    FontContainer::~FontContainer() = default;

    std::optional<size_t> FontContainer::FindOrBake(const std::string& path, uint32_t pixelSize)
    {
        if (path.empty() || pixelSize == 0)
            return std::nullopt;

        const std::string normalized = NormalizeFontPath(path);
        const auto        found      = std::find_if(m_Entries.begin(), m_Entries.end(), [&](const Entry& entry)
                                                    { return entry.PixelSize == pixelSize && entry.Path == normalized; });
        if (found == m_Entries.end())
        {
            Entry entry;
            entry.Path      = normalized;
            entry.PixelSize = pixelSize;
            if (auto font = ContentLoader::LoadFont(normalized, static_cast<float>(pixelSize), m_FileSystem))
            {
                entry.Font = std::make_unique<ContentLoader::LoadedFont>(std::move(*font));
                m_Loaded.push_back(m_Entries.size());
            }
            m_Entries.push_back(std::move(entry));
            if (!m_Entries.back().Font)
                return std::nullopt;
            return m_Loaded.size() - 1;
        }

        if (!found->Font)
            return std::nullopt;
        const size_t entryIndex = static_cast<size_t>(found - m_Entries.begin());
        return static_cast<size_t>(std::find(m_Loaded.begin(), m_Loaded.end(), entryIndex) - m_Loaded.begin());
    }

    size_t FontContainer::GetFontCount() const
    {
        return m_Loaded.size();
    }

    const ContentLoader::LoadedFont& FontContainer::GetFont(size_t index) const
    {
        return *m_Entries[m_Loaded[index]].Font;
    }
}
