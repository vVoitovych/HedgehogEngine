#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/Containers/FontContainer.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"

#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

using HedgehogEngine::FontContainer;

namespace
{
    // ContentLoader's test font, Karla-Regular.ttf (SIL OFL 1.1), mounted as assets://.
    const std::filesystem::path FONT_DIR =
        std::filesystem::path(__FILE__).parent_path() / "../../../ContentLoader/tests/data";

    struct FontFiles
    {
        FS::FileSystemManager Manager;

        FontFiles()
        {
            auto fileSystem = std::make_unique<FS::FileSystem>();
            fileSystem->RegisterPath("assets://", FONT_DIR);
            REQUIRE(Manager.Register(std::move(fileSystem)));
        }
    };

    // Counts the console lines holding fragment while alive. Test code only.
    class ConsoleLines
    {
    public:
        ConsoleLines() : m_Previous(std::cout.rdbuf(m_Captured.rdbuf())) {}
        ~ConsoleLines() { std::cout.rdbuf(m_Previous); }

        ConsoleLines(const ConsoleLines&)            = delete;
        ConsoleLines& operator=(const ConsoleLines&) = delete;

        [[nodiscard]] size_t Count(const std::string& fragment) const
        {
            std::istringstream lines(m_Captured.str());
            std::string        line;
            size_t             count = 0;
            while (std::getline(lines, line))
                count += line.find(fragment) != std::string::npos ? 1 : 0;
            return count;
        }

    private:
        std::ostringstream m_Captured;
        std::streambuf*    m_Previous;
    };
}

TEST_CASE("FontContainer - one font and size bake once and share an atlas; another size bakes again")
{
    REQUIRE(std::filesystem::exists(FONT_DIR / "Karla-Regular.ttf"));
    FontFiles     files;
    FontContainer fonts(files.Manager);

    const auto first = fonts.FindOrBake("Karla-Regular.ttf", 24);
    REQUIRE(first.has_value());
    CHECK(fonts.GetFontCount() == 1);
    CHECK(fonts.GetFont(*first).PixelHeight == 24.0f);

    // The same font, however it is spelled, is the same atlas.
    CHECK(fonts.FindOrBake("Karla-Regular.ttf", 24) == first);
    CHECK(fonts.FindOrBake("assets://Karla-Regular.ttf", 24) == first);
    CHECK(fonts.GetFontCount() == 1);

    const auto larger = fonts.FindOrBake("Karla-Regular.ttf", 48);
    REQUIRE(larger.has_value());
    CHECK(*larger != *first);
    CHECK(fonts.GetFontCount() == 2);
    CHECK(fonts.GetFont(*larger).Ascent == doctest::Approx(fonts.GetFont(*first).Ascent * 2.0f));
}

TEST_CASE("FontContainer - a font that does not load is reported once and never retried")
{
    FontFiles     files;
    FontContainer fonts(files.Manager);

    ConsoleLines console;
    CHECK_FALSE(fonts.FindOrBake("Missing.ttf", 24).has_value());
    CHECK_FALSE(fonts.FindOrBake("Missing.ttf", 24).has_value());
    CHECK(console.Count("Failed to read font file") == 1);
    CHECK_FALSE(fonts.FindOrBake("", 24).has_value());
    CHECK_FALSE(fonts.FindOrBake("Karla-Regular.ttf", 0).has_value());
    CHECK(fonts.GetFontCount() == 0);

    // A good font after the failures still gets index 0.
    CHECK(fonts.FindOrBake("Karla-Regular.ttf", 16) == std::optional<size_t>(0));
}

TEST_CASE("ResourceCatalog - its fonts are the renderer's font atlases")
{
    FontFiles                       files;
    HedgehogEngine::ResourceCatalog catalog(files.Manager);
    // As the renderer reads it: through the interface, whose overrides the DLL does not export.
    const HedgehogEngine::IResourceCatalog& view = catalog;
    CHECK(view.GetFontCount() == 0);

    REQUIRE(catalog.GetFontContainer().FindOrBake("Karla-Regular.ttf", 20).has_value());
    REQUIRE(view.GetFontCount() == 1);
    const HedgehogEngine::FontAtlasView atlas = view.GetFontAtlas(0);
    const ContentLoader::LoadedFont&    font  = catalog.GetFontContainer().GetFont(0);
    CHECK(atlas.AtlasWidth == font.AtlasWidth);
    CHECK(atlas.AtlasHeight == font.AtlasHeight);
    CHECK(atlas.Atlas.data() == font.Atlas.data());
    CHECK(atlas.Atlas.size() == static_cast<size_t>(font.AtlasWidth) * font.AtlasHeight);
}
