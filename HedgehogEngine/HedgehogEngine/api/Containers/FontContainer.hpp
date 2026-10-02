#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "ContentLoader/api/LoadedFont.hpp"
#include "FileSystem/api/FileSystemManager.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace HedgehogEngine
{
    // Baked fonts, one per (path, pixel size): the game UI's text bakes a font the first time it is
    // drawn at a size, and every later text with the same font and size shares that atlas. Fonts are
    // never unloaded; indices stay valid for the container's lifetime.
    class FontContainer
    {
    public:
        HEDGEHOG_ENGINE_API explicit FontContainer(const FS::FileSystemManager& fileSystem);
        HEDGEHOG_ENGINE_API ~FontContainer();

        FontContainer(const FontContainer&)            = delete;
        FontContainer(FontContainer&&)                 = delete;
        FontContainer& operator=(const FontContainer&) = delete;
        FontContainer& operator=(FontContainer&&)      = delete;

        // The index of path (under assets://, the prefix optional, either slash) baked at pixelSize,
        // baking it on first use. std::nullopt for an empty path, a zero size, or a font that does not
        // load, which is logged once and not retried.
        HEDGEHOG_ENGINE_API std::optional<size_t> FindOrBake(const std::string& path, uint32_t pixelSize);

        HEDGEHOG_ENGINE_API size_t                           GetFontCount() const;
        HEDGEHOG_ENGINE_API const ContentLoader::LoadedFont& GetFont(size_t index) const;

    private:
        struct Entry
        {
            std::string                                Path; // normalized
            uint32_t                                   PixelSize = 0;
            std::unique_ptr<ContentLoader::LoadedFont> Font;  // null when it failed to load
        };

        const FS::FileSystemManager& m_FileSystem;
        std::vector<Entry>           m_Entries; // every key tried, failures included
        std::vector<size_t>          m_Loaded;  // font index -> entry index
    };
}
