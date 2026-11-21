#include "doctest/doctest/doctest.h"

#include "ContentLoader/api/MeshLoader.hpp"
#include "ContentLoader/api/MeshLods.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <numbers>

namespace
{
    struct TestMesh
    {
        std::vector<HM::Vector3> Positions;
        std::vector<uint32_t>    Indices;
    };

    // A UV sphere of stacks x slices quads, each two triangles, sharing vertices.
    TestMesh MakeSphere(uint32_t stacks, uint32_t slices)
    {
        TestMesh mesh;
        constexpr float PI = std::numbers::pi_v<float>;
        for (uint32_t i = 0; i <= stacks; ++i)
            for (uint32_t j = 0; j <= slices; ++j)
            {
                const float theta = PI * static_cast<float>(i) / static_cast<float>(stacks);
                const float phi   = 2.0f * PI * static_cast<float>(j) / static_cast<float>(slices);
                mesh.Positions.emplace_back(std::sin(theta) * std::cos(phi), std::sin(theta) * std::sin(phi), std::cos(theta));
            }
        for (uint32_t i = 0; i < stacks; ++i)
            for (uint32_t j = 0; j < slices; ++j)
            {
                const uint32_t a = i * (slices + 1) + j;
                const uint32_t b = a + slices + 1;
                mesh.Indices.insert(mesh.Indices.end(), { a, b, a + 1, a + 1, b, b + 1 });
            }
        return mesh;
    }

    // A flat n x n grid of quads in the xy plane.
    TestMesh MakeGrid(uint32_t n)
    {
        TestMesh mesh;
        for (uint32_t y = 0; y <= n; ++y)
            for (uint32_t x = 0; x <= n; ++x)
                mesh.Positions.emplace_back(static_cast<float>(x), static_cast<float>(y), 0.0f);
        for (uint32_t y = 0; y < n; ++y)
            for (uint32_t x = 0; x < n; ++x)
            {
                const uint32_t a = y * (n + 1) + x;
                const uint32_t b = a + n + 1;
                mesh.Indices.insert(mesh.Indices.end(), { a, b, a + 1, a + 1, b, b + 1 });
            }
        return mesh;
    }

    void CheckValidTriangles(const std::vector<uint32_t>& indices, size_t vertexCount)
    {
        CHECK(indices.size() % 3 == 0);
        for (size_t i = 0; i < indices.size(); i += 3)
        {
            CHECK(indices[i] < vertexCount);
            CHECK(indices[i + 1] < vertexCount);
            CHECK(indices[i + 2] < vertexCount);
            CHECK((indices[i] != indices[i + 1] && indices[i + 1] != indices[i + 2] && indices[i] != indices[i + 2]));
        }
    }
}

TEST_CASE("A dense sphere gets three strictly shrinking levels of valid triangles")
{
    const TestMesh sphere = MakeSphere(64, 128); // 16384 triangles
    const auto     levels = ContentLoader::GenerateMeshLods(sphere.Positions, sphere.Indices);
    REQUIRE(levels.size() == 3);
    size_t previous = sphere.Indices.size();
    for (const auto& level : levels)
    {
        CHECK(level.size() <= previous * 4 / 5);
        CHECK(level.size() > 0);
        CheckValidTriangles(level, sphere.Positions.size());
        previous = level.size();
    }
}

TEST_CASE("A mesh under the minimum, or with malformed indices, gets no levels")
{
    const TestMesh small = MakeSphere(16, 16); // 512 triangles
    CHECK(ContentLoader::GenerateMeshLods(small.Positions, small.Indices).empty());

    TestMesh broken = MakeSphere(64, 128);
    broken.Indices.pop_back();
    CHECK(ContentLoader::GenerateMeshLods(broken.Positions, broken.Indices).empty());

    TestMesh outOfRange = MakeSphere(64, 128);
    outOfRange.Indices[7] = static_cast<uint32_t>(outOfRange.Positions.size());
    CHECK(ContentLoader::GenerateMeshLods(outOfRange.Positions, outOfRange.Indices).empty());
}

TEST_CASE("A flat grid collapses to a few triangles")
{
    const TestMesh grid   = MakeGrid(48); // 4608 triangles
    const auto     levels = ContentLoader::GenerateMeshLods(grid.Positions, grid.Indices);
    REQUIRE_FALSE(levels.empty());
    CHECK(levels.back().size() / 3 <= grid.Indices.size() / 3 / 8);
    for (const auto& level : levels)
        CheckValidTriangles(level, grid.Positions.size());
}

TEST_CASE("The same mesh gives the same levels")
{
    const TestMesh sphere = MakeSphere(64, 128);
    CHECK(ContentLoader::GenerateMeshLods(sphere.Positions, sphere.Indices) ==
          ContentLoader::GenerateMeshLods(sphere.Positions, sphere.Indices));
}

TEST_CASE("DamagedHelmet's levels, timed")
{
    // tests/ -> ContentLoader/ -> repository root.
    const std::filesystem::path assets =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Projects/FeatureTest/Assets";
    FS::FileSystemManager fileSystem;
    auto                  fs = std::make_unique<FS::FileSystem>();
    REQUIRE(fs->RegisterPath("assets://", assets));
    REQUIRE(fileSystem.Register(std::move(fs)));

    const auto mesh = ContentLoader::LoadMesh("Models/DamagedHelmet/DamagedHelmet.gltf", fileSystem);
    REQUIRE(mesh.has_value());
    std::vector<HM::Vector3> positions;
    for (const auto& vertex : mesh->vertices)
        positions.push_back(vertex.position);

    const auto start  = std::chrono::steady_clock::now();
    const auto levels = ContentLoader::GenerateMeshLods(positions, mesh->indices);
    const auto ms     = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();

    REQUIRE_FALSE(levels.empty());
    std::cout << "DamagedHelmet: " << mesh->indices.size() / 3 << " triangles";
    for (const auto& level : levels)
    {
        CheckValidTriangles(level, positions.size());
        std::cout << ", " << level.size() / 3;
    }
    std::cout << " in " << ms << " ms\n";
}
