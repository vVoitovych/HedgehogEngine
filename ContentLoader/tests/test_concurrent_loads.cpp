#include "doctest/doctest/doctest.h"

#include "test_hdr_files.hpp"

#include "ContentLoader/api/EnvironmentBake.hpp"
#include "ContentLoader/api/MeshLoader.hpp"
#include "ContentLoader/api/TextureLoader.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include <atomic>
#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// The evidence background loading relies on: loads share one FileSystemManager (whose reads are
// const) and the loaders keep no global state (stb's flip is never set; tinygltf keeps none), so two
// threads loading at once get exactly what one thread does.
namespace
{
    // FeatureTest's skinned strip, a small glTF with its own buffer.
    const std::filesystem::path STRIP = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() /
                                        "Projects/FeatureTest/Assets/Models/Animated/TwoBoneStrip.gltf";

    constexpr const char* QUAD_OBJ = "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
                                     "vn 0 0 1\nf 1/1/1 2/2/1 3/3/1\nf 1/1/1 3/3/1 4/4/1\n";

    // A 16x16 binary PPM of a gradient.
    std::string GradientPpm()
    {
        std::string ppm = "P6\n16 16\n255\n";
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
                for (const int value : { x * 16, y * 16, (x + y) * 8 })
                    ppm.push_back(static_cast<char>(value));
        return ppm;
    }

    // What one pass of loads gives, compared field by field.
    struct Loaded
    {
        std::optional<ContentLoader::LoadedMesh> Obj;
        std::optional<ContentLoader::LoadedMesh> Gltf;
        std::vector<unsigned char>               Texture;
        std::optional<ContentLoader::HdrImage>   Hdr;
    };

    Loaded LoadAll(const FS::FileSystemManager& files)
    {
        Loaded loaded;
        loaded.Obj  = ContentLoader::LoadMesh("Models/quad.obj", files);
        loaded.Gltf = ContentLoader::LoadMesh("Models/TwoBoneStrip.gltf", files);
        ContentLoader::TextureLoader texture;
        if (texture.LoadTexture("Textures/gradient.ppm", files))
        {
            const size_t size = static_cast<size_t>(texture.GetWidth()) * texture.GetHeight() * 4;
            const auto*  data = static_cast<const unsigned char*>(texture.GetData());
            loaded.Texture.assign(data, data + size);
        }
        loaded.Hdr = ContentLoader::LoadHdrImage("Environments/sky.hdr", files);
        return loaded;
    }

    bool SameMesh(const std::optional<ContentLoader::LoadedMesh>& a, const std::optional<ContentLoader::LoadedMesh>& b)
    {
        if (!a || !b)
            return false;
        if (a->vertices != b->vertices || a->indices != b->indices || a->Joints.size() != b->Joints.size() ||
            a->Weights.size() != b->Weights.size() || a->Skin.has_value() != b->Skin.has_value())
            return false;
        for (size_t i = 0; i < a->Weights.size(); ++i)
            if (std::memcmp(&a->Weights[i], &b->Weights[i], sizeof(a->Weights[i])) != 0 ||
                std::memcmp(&a->Joints[i], &b->Joints[i], sizeof(a->Joints[i])) != 0)
                return false;
        return true;
    }

    bool Same(const Loaded& a, const Loaded& b)
    {
        return SameMesh(a.Obj, b.Obj) && SameMesh(a.Gltf, b.Gltf) && a.Texture == b.Texture && a.Hdr && b.Hdr &&
               a.Hdr->Width == b.Hdr->Width && a.Hdr->Pixels == b.Hdr->Pixels;
    }
}

TEST_CASE("Concurrent loads - two threads loading OBJ, glTF, a texture and an HDR match one thread")
{
    TempDir dir;
    dir.WriteFile("Models/quad.obj", QUAD_OBJ);
    std::filesystem::copy_file(STRIP, dir.Path() / "Models/TwoBoneStrip.gltf");
    dir.WriteFile("Textures/gradient.ppm", GradientPpm());
    ContentLoader::HdrImage sky = HdrTest::ConstantImage(32, 16, HM::Vector3(0.2f, 0.4f, 0.8f));
    HdrTest::SetTexel(sky, 3, 2, HM::Vector3(6.0f, 5.0f, 4.0f));
    dir.WriteFile("Environments/sky.hdr", HdrTest::WriteRadiance(sky));

    FS::FileSystemManager files;
    auto                  fs = std::make_unique<FS::FileSystem>();
    REQUIRE(fs->RegisterPath("assets://", dir.Path()));
    REQUIRE(files.Register(std::move(fs)));

    const Loaded expected = LoadAll(files);
    REQUIRE(expected.Obj);
    REQUIRE(expected.Gltf);
    REQUIRE(expected.Gltf->Skin);
    REQUIRE(expected.Texture.size() == 16 * 16 * 4);
    REQUIRE(expected.Hdr);

    constexpr int    ITERATIONS = 50;
    std::atomic<int> mismatches{ 0 };
    auto             load = [&]()
    {
        for (int i = 0; i < ITERATIONS; ++i)
            if (!Same(LoadAll(files), expected))
                ++mismatches;
    };
    std::thread first(load);
    std::thread second(load);
    first.join();
    second.join();
    CHECK(mismatches == 0);
}
