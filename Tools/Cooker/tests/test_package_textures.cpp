#include "doctest/doctest/doctest.h"

#include "CookPlan.hpp"
#include "Package.hpp"

#include "test_cook_fixture.hpp"

#include "ContentLoader/api/CompressedTexture.hpp"

#include "HedgehogEngine/api/Terrain/TerrainData.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace CookTest;
using ContentLoader::CompressedFormat;

namespace
{
    // An uncompressed 32-bit TGA, rows top first, of a gradient shifted by seed.
    std::string MakeTga(uint32_t width, uint32_t height, uint8_t seed)
    {
        std::string bytes(18, '\0');
        bytes[2]  = 2; // uncompressed true colour
        bytes[12] = static_cast<char>(width & 0xff);
        bytes[13] = static_cast<char>(width >> 8);
        bytes[14] = static_cast<char>(height & 0xff);
        bytes[15] = static_cast<char>(height >> 8);
        bytes[16] = 32;
        bytes[17] = 0x28; // top-left origin, 8 alpha bits
        for (uint32_t y = 0; y < height; ++y)
            for (uint32_t x = 0; x < width; ++x)
            {
                bytes.push_back(static_cast<char>(x * 255 / width + seed)); // B
                bytes.push_back(static_cast<char>(y * 255 / height));       // G
                bytes.push_back(static_cast<char>(128));                    // R
                bytes.push_back(static_cast<char>(255));                    // A
            }
        return bytes;
    }

    constexpr const char* TERRAIN_LEVEL = R"(Version: 1
Scene name: Level
Scene:
  - Entity: 0
    Name: Root
    Parent: 0
    Children:
      - Entity: 1
        Name: Crate
        Parent: 0
        MeshComponent:
          MeshPath: Models\crate.obj
        RenderComponent:
          Material: Materials\Crate.material
        Children: []
      - Entity: 2
        Name: Hills
        Parent: 0
        TerrainComponent:
          Terrain: Terrain/Hills.terrain
        Children: []
)";

    // The fixture project with real images: the crate's material names a colour map, a normal map
    // and a metallic-roughness map, a second material names the colour map as data, and a terrain
    // names a layer's colour map.
    struct TextureFixture : FixtureProject
    {
        TempDir Binaries;

        TextureFixture()
        {
            for (const char* licence : Cooker::LICENCE_FILES)
                Project.WriteFile(licence, std::string("text of ") + licence);
            for (const char* file : Cooker::GAME_RUNTIME_FILES)
                Binaries.WriteFile(file, std::string("binary ") + file);

            Project.WriteFile("Assets/Scenes/Level.yaml", TERRAIN_LEVEL);
            Project.WriteFile("Assets/Textures/crate.tga", MakeTga(16, 8, 0));
            Project.WriteFile("Assets/Textures/crate_n.tga", MakeTga(8, 8, 3));
            Project.WriteFile("Assets/Textures/crate_mr.tga", MakeTga(4, 4, 7));
            Project.WriteFile("Assets/Textures/grass.tga", MakeTga(8, 4, 9));
            WriteCrateMaterial(true);

            HedgehogEngine::TerrainData terrain = HedgehogEngine::MakeDefaultTerrain();
            terrain.Layers[0].BaseColor         = "Textures/grass.tga";
            Project.WriteFile("Assets/Terrain/Hills.terrain", HedgehogEngine::WriteTerrain(terrain));
        }

        void WriteCrateMaterial(bool withNormalMap) const
        {
            std::string text = "Type: 0\nBaseColor: Textures/crate.tga\nTransparency: 1\nMetallicRoughnessMap: Textures/crate_mr.tga\n";
            if (withNormalMap)
                text += "NormalMap: Textures/crate_n.tga\n";
            Project.WriteFile("Assets/Materials/Crate.material", text);
        }

        Cooker::PackageDesc Desc(bool compress = true) const
        {
            Cooker::PackageDesc desc{ Project.Path(), Project.Path(), Out.Path(), Binaries.Path(), {}, {} };
            desc.CompressTextures = compress;
            return desc;
        }

        std::vector<std::string> Siblings() const
        {
            std::vector<std::string> dds;
            for (const std::string& file : PackagedFiles())
                if (file.ends_with(".dds"))
                    dds.push_back(file);
            return dds;
        }

        ContentLoader::DdsReadResult ReadSibling(const std::string& file) const
        {
            std::ifstream          in(Out.Path() / file, std::ios::binary);
            const std::string      text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            std::vector<std::byte> bytes(text.size());
            std::transform(text.begin(), text.end(), bytes.begin(), [](char c) { return static_cast<std::byte>(c); });
            return ContentLoader::ParseDds(bytes);
        }
    };
}

TEST_CASE("Package - every material and terrain map gets a compressed sibling of its encoding")
{
    TextureFixture fixture;
    const Cooker::PackageResult result = Cooker::PackageGame(fixture.Desc());
    REQUIRE(result.Errors.empty());
    CHECK(result.TexturesCompressed == 4);
    CHECK(fixture.Siblings() == std::vector<std::string>{
                                    "Assets/Textures/crate.tga.bc7.dds",
                                    "Assets/Textures/crate_mr.tga.bc7lin.dds",
                                    "Assets/Textures/crate_n.tga.bc5.dds",
                                    "Assets/Textures/grass.tga.bc7.dds",
                                });

    const ContentLoader::DdsReadResult colour = fixture.ReadSibling("Assets/Textures/crate.tga.bc7.dds");
    REQUIRE(colour.Texture.has_value());
    CHECK(colour.Texture->Format == CompressedFormat::BC7Srgb);
    CHECK(colour.Texture->Width == 16);
    CHECK(colour.Texture->Height == 8);
    CHECK(colour.Texture->Mips.size() == 5); // 16x8 .. 1x1
    const ContentLoader::DdsReadResult normal = fixture.ReadSibling("Assets/Textures/crate_n.tga.bc5.dds");
    REQUIRE(normal.Texture.has_value());
    CHECK(normal.Texture->Format == CompressedFormat::BC5Unorm);
    const ContentLoader::DdsReadResult data = fixture.ReadSibling("Assets/Textures/crate_mr.tga.bc7lin.dds");
    REQUIRE(data.Texture.has_value());
    CHECK(data.Texture->Format == CompressedFormat::BC7Unorm);

    // The images themselves still ship.
    const std::vector<std::string> files = fixture.PackagedFiles();
    CHECK(std::find(files.begin(), files.end(), "Assets/Textures/crate.tga") != files.end());
}

TEST_CASE("Package - a repackage encodes only the images that changed")
{
    TextureFixture fixture;
    REQUIRE(Cooker::PackageGame(fixture.Desc()).Errors.empty());

    const Cooker::PackageResult again = Cooker::PackageGame(fixture.Desc());
    REQUIRE(again.Errors.empty());
    CHECK(again.TexturesCompressed == 0);
    CHECK(again.Copied == 0);

    fixture.Project.WriteFile("Assets/Textures/crate_n.tga", MakeTga(8, 8, 40));
    const Cooker::PackageResult touched = Cooker::PackageGame(fixture.Desc());
    REQUIRE(touched.Errors.empty());
    CHECK(touched.TexturesCompressed == 1);
    CHECK(touched.Copied == 1); // the image itself

    // A deleted sibling is written again.
    std::filesystem::remove(fixture.Out.Path() / "Assets/Textures/grass.tga.bc7.dds");
    CHECK(Cooker::PackageGame(fixture.Desc()).TexturesCompressed == 1);
}

TEST_CASE("Package - a map no longer named loses its sibling")
{
    TextureFixture fixture;
    REQUIRE(Cooker::PackageGame(fixture.Desc()).Errors.empty());
    fixture.WriteCrateMaterial(false);
    REQUIRE(Cooker::PackageGame(fixture.Desc()).Errors.empty());
    const std::vector<std::string> siblings = fixture.Siblings();
    CHECK(std::find(siblings.begin(), siblings.end(), "Assets/Textures/crate_n.tga.bc5.dds") == siblings.end());
    CHECK(siblings.size() == 3);
}

TEST_CASE("Package - one image used two ways gets a sibling for each")
{
    TextureFixture fixture;
    fixture.Project.WriteFile("Assets/Materials/Data.material", "Type: 0\nOcclusionMap: Textures/crate.tga\n");
    fixture.Project.WriteFile("Assets/Scenes/Level.yaml", std::string(TERRAIN_LEVEL) +
                                                              "      - Entity: 3\n        Name: Other\n        Parent: 0\n"
                                                              "        MeshComponent:\n          MeshPath: Models\\crate.obj\n"
                                                              "        RenderComponent:\n          Material: Materials\\Data.material\n"
                                                              "        Children: []\n");
    REQUIRE(Cooker::PackageGame(fixture.Desc()).Errors.empty());
    const std::vector<std::string> siblings = fixture.Siblings();
    CHECK(std::find(siblings.begin(), siblings.end(), "Assets/Textures/crate.tga.bc7.dds") != siblings.end());
    CHECK(std::find(siblings.begin(), siblings.end(), "Assets/Textures/crate.tga.bc7lin.dds") != siblings.end());
}

TEST_CASE("Package - compression turned off writes no sibling")
{
    TextureFixture fixture;
    const Cooker::PackageResult result = Cooker::PackageGame(fixture.Desc(false));
    REQUIRE(result.Errors.empty());
    CHECK(result.TexturesCompressed == 0);
    CHECK(fixture.Siblings().empty());
}

TEST_CASE("Package - an image that does not decode fails naming it, copying nothing")
{
    TextureFixture fixture;
    fixture.Project.WriteFile("Assets/Textures/crate_n.tga", "not an image");
    const Cooker::PackageResult result = Cooker::PackageGame(fixture.Desc());
    REQUIRE(result.Errors.size() == 1);
    CHECK(result.Errors[0] == "assets://Textures/crate_n.tga cannot be decoded as an image to compress.");
    CHECK(fixture.PackagedFiles().empty());
}
