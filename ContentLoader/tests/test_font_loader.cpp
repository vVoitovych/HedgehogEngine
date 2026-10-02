#include "doctest/doctest/doctest.h"

#include "ContentLoader/api/FontLoader.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"
#include "HedgehogScripting/tests/test_log_capture.hpp"

#include <algorithm>
#include <filesystem>
#include <tuple>

using ContentLoader::LoadedFont;
using ContentLoader::LoadedGlyph;

namespace
{
    // tests/data, beside this file: Karla-Regular.ttf (SIL Open Font License 1.1, see README.md there).
    const std::filesystem::path DATA_DIR = std::filesystem::path(__FILE__).parent_path() / "data";

    void MountAssets(FS::FileSystemManager& manager, const std::filesystem::path& dir)
    {
        auto fs = std::make_unique<FS::FileSystem>();
        fs->RegisterPath("assets://", dir);
        REQUIRE(manager.Register(std::move(fs)));
    }

    const LoadedGlyph* Find(const LoadedFont& font, uint32_t codepoint)
    {
        const auto found = std::find_if(font.Glyphs.begin(), font.Glyphs.end(),
                                        [&](const LoadedGlyph& glyph) { return glyph.Codepoint == codepoint; });
        return found != font.Glyphs.end() ? &*found : nullptr;
    }

    // Whether two glyphs' atlas rects, each grown by one pixel of padding, overlap.
    bool Overlap(const LoadedGlyph& a, const LoadedGlyph& b)
    {
        return a.AtlasX < b.AtlasX + b.Width + 1 && b.AtlasX < a.AtlasX + a.Width + 1 &&
               a.AtlasY < b.AtlasY + b.Height + 1 && b.AtlasY < a.AtlasY + a.Height + 1;
    }
}

TEST_CASE("LoadFont - bakes every printable ASCII glyph and the font's Latin-1 into one atlas without overlap")
{
    REQUIRE(std::filesystem::exists(DATA_DIR / "Karla-Regular.ttf"));
    FS::FileSystemManager fileSystem;
    MountAssets(fileSystem, DATA_DIR);

    const auto loaded = ContentLoader::LoadFont("Karla-Regular.ttf", 32.0f, fileSystem);
    REQUIRE(loaded.has_value());
    const LoadedFont& font = *loaded;

    CHECK(font.PixelHeight == 32.0f);
    CHECK(font.Ascent > 0.0f);
    CHECK(font.Descent < 0.0f);
    CHECK(font.Ascent - font.Descent == doctest::Approx(32.0f));

    for (uint32_t codepoint = 0x20; codepoint <= 0x7E; ++codepoint)
    {
        CAPTURE(codepoint);
        CHECK(Find(font, codepoint) != nullptr);
    }
    CHECK(Find(font, 0xE9) != nullptr); // é
    CHECK(Find(font, 0x7F) == nullptr); // DEL is not printable
    CHECK(std::is_sorted(font.Glyphs.begin(), font.Glyphs.end(),
                         [](const LoadedGlyph& a, const LoadedGlyph& b) { return a.Codepoint < b.Codepoint; }));
    REQUIRE(font.FallbackGlyph < font.Glyphs.size());
    CHECK(font.Glyphs[font.FallbackGlyph].Codepoint == '?');

    // A space has an advance and no pixels; a letter has both, and its bitmap sits above the baseline.
    const LoadedGlyph& space = *Find(font, ' ');
    CHECK(space.Advance > 0.0f);
    CHECK(space.Width * space.Height == 0);
    const LoadedGlyph& h = *Find(font, 'H');
    CHECK(h.Width > 0);
    CHECK(h.Height > 0);
    CHECK(h.OffsetY < 0.0f);
    CHECK(h.Advance >= static_cast<float>(h.Width));

    // Every bitmap lies inside the atlas, apart from every other one.
    REQUIRE(font.Atlas.size() == static_cast<size_t>(font.AtlasWidth) * font.AtlasHeight);
    std::vector<const LoadedGlyph*> drawn;
    for (const LoadedGlyph& glyph : font.Glyphs)
    {
        if (glyph.Width == 0 || glyph.Height == 0)
            continue;
        CHECK(glyph.AtlasX + glyph.Width <= font.AtlasWidth);
        CHECK(glyph.AtlasY + glyph.Height <= font.AtlasHeight);
        drawn.push_back(&glyph);
    }
    CHECK(drawn.size() >= 0x7E - 0x20); // every printable ASCII glyph but the space has pixels
    size_t overlaps = 0;
    for (size_t i = 0; i < drawn.size(); ++i)
        for (size_t j = i + 1; j < drawn.size(); ++j)
            overlaps += Overlap(*drawn[i], *drawn[j]) ? 1 : 0;
    CHECK(overlaps == 0);

    // The glyphs were rasterized into their rects: 'H' has full-coverage pixels.
    uint8_t brightest = 0;
    for (uint32_t y = 0; y < h.Height; ++y)
        for (uint32_t x = 0; x < h.Width; ++x)
            brightest = std::max(brightest, font.Atlas[(h.AtlasY + y) * font.AtlasWidth + h.AtlasX + x]);
    CHECK(brightest == 255);

    // Karla kerns through GPOS alone (it has no kern table); the pairs are sorted and refer to baked glyphs.
    CHECK_FALSE(font.Kerning.empty());
    CHECK(std::is_sorted(font.Kerning.begin(), font.Kerning.end(),
                         [](const auto& a, const auto& b) { return std::tie(a.Left, a.Right) < std::tie(b.Left, b.Right); }));
    for (const ContentLoader::LoadedKerningPair& pair : font.Kerning)
    {
        CHECK(pair.Left < font.Glyphs.size());
        CHECK(pair.Right < font.Glyphs.size());
        CHECK(pair.Advance != 0.0f);
    }
}

TEST_CASE("LoadFont - the same font at twice the size has twice the metrics")
{
    FS::FileSystemManager fileSystem;
    MountAssets(fileSystem, DATA_DIR);
    const auto small = ContentLoader::LoadFont("Karla-Regular.ttf", 16.0f, fileSystem);
    const auto large = ContentLoader::LoadFont("Karla-Regular.ttf", 32.0f, fileSystem);
    REQUIRE(small.has_value());
    REQUIRE(large.has_value());
    CHECK(large->Ascent == doctest::Approx(small->Ascent * 2.0f));
    CHECK(Find(*large, 'W')->Advance == doctest::Approx(Find(*small, 'W')->Advance * 2.0f));
    CHECK(large->Glyphs.size() == small->Glyphs.size());
}

TEST_CASE("LoadFont - a missing file, a file that is not a font and a bad size each fail with an error")
{
    TempDir tmp;
    tmp.WriteFile("notafont.ttf", "This is plain text, not a TrueType font.");
    tmp.WriteFile("tiny.ttf", "abc");
    FS::FileSystemManager fileSystem;
    MountAssets(fileSystem, tmp.Path());

    LogCapture capture;
    CHECK_FALSE(ContentLoader::LoadFont("missing.ttf", 16.0f, fileSystem).has_value());
    CHECK_FALSE(ContentLoader::LoadFont("notafont.ttf", 16.0f, fileSystem).has_value());
    CHECK_FALSE(ContentLoader::LoadFont("tiny.ttf", 16.0f, fileSystem).has_value());
    CHECK_FALSE(ContentLoader::LoadFont("notafont.ttf", 0.0f, fileSystem).has_value());
    const std::string output = capture.Text();
    CHECK(output.find("missing.ttf") != std::string::npos);
    CHECK(output.find("notafont.ttf is not a TrueType font.") != std::string::npos);
    CHECK(output.find("pixel height of 0") != std::string::npos);
}
