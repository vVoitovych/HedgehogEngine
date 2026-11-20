#include "doctest/doctest/doctest.h"

#include "test_hdr_files.hpp"

#include "ContentLoader/api/AssetPrefetch.hpp"
#include "ContentLoader/api/EnvironmentBake.hpp"
#include "ContentLoader/api/MeshLoader.hpp"
#include "ContentLoader/api/TextureLoader.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include "HedgehogScripting/tests/test_log_capture.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace
{
    constexpr const char* TRIANGLE_OBJ = "v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nvn 0 0 1\n"
                                         "f 1/1/1 2/2/1 3/3/1\n";

    // A 4x2 binary PPM.
    std::string SmallPpm()
    {
        std::string ppm = "P6\n4 2\n255\n";
        for (int i = 0; i < 8; ++i)
            for (const int value : { i * 30, 255 - i * 30, 7 * i })
                ppm.push_back(static_cast<char>(value));
        return ppm;
    }

    const ContentLoader::EnvironmentBakeDesc SMALL_BAKE{ 8, 4 };

    // A temp assets:// holding a mesh, a texture and a sky; the store is emptied on both sides.
    struct PrefetchWorld
    {
        TempDir               Dir;
        FS::FileSystemManager Files;

        PrefetchWorld()
        {
            ContentLoader::ClearPrefetched();
            Dir.WriteFile("Models/triangle.obj", TRIANGLE_OBJ);
            Dir.WriteFile("Textures/small.ppm", SmallPpm());
            Dir.WriteFile("Environments/sky.hdr",
                          HdrTest::WriteRadiance(HdrTest::ConstantImage(16, 8, HM::Vector3(0.5f, 0.6f, 0.7f))));
            auto fs = std::make_unique<FS::FileSystem>();
            REQUIRE(fs->RegisterPath("assets://", Dir.Path()));
            REQUIRE(Files.Register(std::move(fs)));
        }

        ~PrefetchWorld() { ContentLoader::ClearPrefetched(); }

        void DeleteFiles() const
        {
            std::filesystem::remove(Dir.Path() / "Models/triangle.obj");
            std::filesystem::remove(Dir.Path() / "Textures/small.ppm");
            std::filesystem::remove(Dir.Path() / "Environments/sky.hdr");
        }
    };

    std::vector<unsigned char> Pixels(const ContentLoader::TextureLoader& texture)
    {
        const auto*  data = static_cast<const unsigned char*>(texture.GetData());
        const size_t size = static_cast<size_t>(texture.GetWidth()) * texture.GetHeight() * 4;
        return std::vector<unsigned char>(data, data + size);
    }
}

TEST_CASE("Asset prefetch - prefetched assets load once with their files gone, equal to a direct load")
{
    PrefetchWorld world;
    const auto    directMesh = ContentLoader::LoadMesh("Models/triangle.obj", world.Files);
    ContentLoader::TextureLoader directTexture;
    REQUIRE(directTexture.LoadTexture("Textures/small.ppm", world.Files));
    const auto directSky = ContentLoader::BakeEnvironment("Environments/sky.hdr", world.Files, SMALL_BAKE);
    REQUIRE(directMesh);
    REQUIRE(directSky);
    CHECK(ContentLoader::GetPrefetchedBytes() == 0);

    REQUIRE(ContentLoader::PrefetchMesh("Models/triangle.obj", world.Files));
    REQUIRE(ContentLoader::PrefetchTexture("Textures/small.ppm", world.Files));
    REQUIRE(ContentLoader::PrefetchEnvironment("Environments/sky.hdr", world.Files, SMALL_BAKE));
    CHECK(ContentLoader::GetPrefetchedBytes() > 0);
    world.DeleteFiles();

    // Any spelling of the path finds the entry.
    const auto mesh = ContentLoader::LoadMesh("assets://Models\\triangle.obj", world.Files);
    REQUIRE(mesh);
    CHECK(mesh->vertices == directMesh->vertices);
    CHECK(mesh->indices == directMesh->indices);

    ContentLoader::TextureLoader texture;
    REQUIRE(texture.LoadTexture("Textures/./small.ppm", world.Files));
    CHECK(texture.GetWidth() == 4);
    CHECK(texture.GetHeight() == 2);
    CHECK(texture.GetChannels() == directTexture.GetChannels());
    CHECK(Pixels(texture) == Pixels(directTexture));

    const auto sky = ContentLoader::BakeEnvironment("Environments/sky.hdr", world.Files, SMALL_BAKE);
    REQUIRE(sky);
    CHECK(sky->MipCount == directSky->MipCount);
    CHECK(sky->Mips == directSky->Mips);
    CHECK(ContentLoader::GetPrefetchedBytes() == 0);

    // Taken once: the second load reads the file, which is gone.
    LogCapture log;
    CHECK_FALSE(ContentLoader::LoadMesh("Models/triangle.obj", world.Files));
    ContentLoader::TextureLoader again;
    CHECK_FALSE(again.LoadTexture("Textures/small.ppm", world.Files));
    CHECK_FALSE(ContentLoader::BakeEnvironment("Environments/sky.hdr", world.Files, SMALL_BAKE));
    CHECK(log.Lines("[ERROR]").size() == 3);
}

TEST_CASE("Asset prefetch - an environment kept for another bake is not taken; Clear frees everything")
{
    PrefetchWorld world;
    REQUIRE(ContentLoader::PrefetchEnvironment("Environments/sky.hdr", world.Files, SMALL_BAKE));
    CHECK_FALSE(ContentLoader::TakeEnvironment("Environments/sky.hdr", { 16, 4 }));
    REQUIRE(ContentLoader::PrefetchMesh("Models/triangle.obj", world.Files));
    REQUIRE(ContentLoader::PrefetchMesh("Models/triangle.obj", world.Files)); // kept already
    REQUIRE(ContentLoader::PrefetchTexture("Textures/small.ppm", world.Files));
    CHECK(ContentLoader::GetPrefetchedBytes() > 0);

    ContentLoader::ClearPrefetched();
    CHECK(ContentLoader::GetPrefetchedBytes() == 0);
    CHECK_FALSE(ContentLoader::TakeMesh("Models/triangle.obj"));
    CHECK_FALSE(ContentLoader::TakeTexture("Textures/small.ppm"));
    CHECK_FALSE(ContentLoader::TakeEnvironment("Environments/sky.hdr", SMALL_BAKE));
}

TEST_CASE("Asset prefetch - a file that does not load keeps nothing")
{
    PrefetchWorld world;
    LogCapture    log;
    CHECK_FALSE(ContentLoader::PrefetchMesh("Models/missing.obj", world.Files));
    CHECK_FALSE(ContentLoader::PrefetchTexture("Textures/missing.png", world.Files));
    CHECK_FALSE(ContentLoader::PrefetchEnvironment("Environments/missing.hdr", world.Files, SMALL_BAKE));
    CHECK(ContentLoader::GetPrefetchedBytes() == 0);
    CHECK(log.Lines("[ERROR]").size() == 3);
}
