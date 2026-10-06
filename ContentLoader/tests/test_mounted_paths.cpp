#include "doctest/doctest/doctest.h"

#include "ContentLoader/api/FontLoader.hpp"
#include "ContentLoader/api/MeshLoader.hpp"
#include "ContentLoader/api/TextureLoader.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include <filesystem>
#include <memory>
#include <string>

// A path without a mount is under assets://; one naming a mount (the engine's own
// engine://Content/...) is read from that mount, through the same calls.
namespace
{
    // tests/data, beside this file: Karla-Regular.ttf (SIL Open Font License 1.1, see README.md there).
    const std::filesystem::path DATA_DIR = std::filesystem::path(__FILE__).parent_path() / "data";

    constexpr const char* TRIANGLE_OBJ = "v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nvn 0 0 1\n"
                                         "f 1/1/1 2/2/1 3/3/1\n";

    // A 2x1 binary PPM: a red and a blue pixel.
    std::string TwoPixelPpm()
    {
        std::string ppm = "P6\n2 1\n255\n";
        for (const unsigned char byte : { 255, 0, 0, 0, 0, 255 })
            ppm.push_back(static_cast<char>(byte));
        return ppm;
    }

    // assets:// at the project's folder and engine:// at the engine's, as EngineContext mounts them.
    struct Mounts
    {
        TempDir               Project;
        TempDir               Engine;
        FS::FileSystemManager Files;

        explicit Mounts(const std::filesystem::path& engineRoot = {})
        {
            auto fs = std::make_unique<FS::FileSystem>();
            REQUIRE(fs->RegisterPath("assets://", Project.Path()));
            REQUIRE(fs->RegisterPath("engine://", engineRoot.empty() ? Engine.Path() : engineRoot));
            REQUIRE(Files.Register(std::move(fs)));
        }
    };
}

TEST_CASE("Mounted paths - a mesh loads from engine:// as from assets://")
{
    Mounts mounts;
    mounts.Project.WriteFile("Models/triangle.obj", TRIANGLE_OBJ);
    mounts.Engine.WriteFile("Content/Models/triangle.obj", TRIANGLE_OBJ);

    const auto project = ContentLoader::LoadMesh("Models/triangle.obj", mounts.Files);
    const auto engine  = ContentLoader::LoadMesh("engine://Content/Models/triangle.obj", mounts.Files);
    REQUIRE(project.has_value());
    REQUIRE(engine.has_value());
    CHECK(engine->vertices.size() == project->vertices.size());
    CHECK(engine->indices == project->indices);

    // An engine file is not found under assets://.
    CHECK_FALSE(ContentLoader::LoadMesh("Content/Models/triangle.obj", mounts.Files).has_value());
}

TEST_CASE("Mounted paths - a texture loads from engine:// as from assets://")
{
    Mounts mounts;
    mounts.Project.WriteFile("Textures/pair.ppm", TwoPixelPpm());
    mounts.Engine.WriteFile("Content/Textures/pair.ppm", TwoPixelPpm());

    ContentLoader::TextureLoader project;
    ContentLoader::TextureLoader engine;
    REQUIRE(project.LoadTexture("Textures/pair.ppm", mounts.Files));
    REQUIRE(engine.LoadTexture("engine://Content/Textures/pair.ppm", mounts.Files));
    CHECK(engine.GetWidth() == 2);
    CHECK(engine.GetHeight() == 1);
    CHECK(project.GetWidth() == engine.GetWidth());
}

TEST_CASE("Mounted paths - a font bakes from engine:// as from assets://")
{
    REQUIRE(std::filesystem::exists(DATA_DIR / "Karla-Regular.ttf"));
    Mounts mounts(DATA_DIR);

    const auto engine = ContentLoader::LoadFont("engine://Karla-Regular.ttf", 24.0f, mounts.Files);
    REQUIRE(engine.has_value());
    CHECK_FALSE(engine->Glyphs.empty());
    CHECK_FALSE(ContentLoader::LoadFont("Karla-Regular.ttf", 24.0f, mounts.Files).has_value());
}
