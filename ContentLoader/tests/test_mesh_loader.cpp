#include "doctest/doctest/doctest.h"

#include "ContentLoader/api/MeshLoader.hpp"
#include "ContentLoader/api/LoadedData.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include <cmath>

namespace
{
    // Single triangle in the XY plane with texcoords and a +Z normal.
    constexpr const char* TRIANGLE_OBJ =
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "vt 0 0\n"
        "vt 1 0\n"
        "vt 0 1\n"
        "vn 0 0 1\n"
        "f 1/1/1 2/2/1 3/3/1\n";

    // Mounts "assets://" (LoadMesh's implicit mount) at the given directory.
    void MountAssets(FS::FileSystemManager& manager, const std::filesystem::path& dir)
    {
        auto fs = std::make_unique<FS::FileSystem>();
        fs->RegisterPath("assets://", dir);
        REQUIRE(manager.Register(std::move(fs)));
    }

    // A unit quad in the XY plane, two triangles over four positions, with the given faces.
    std::optional<ContentLoader::LoadedMesh> LoadQuad(const char* attributes, const char* faces)
    {
        TempDir tmp;
        tmp.WriteFile("quad.obj", std::string("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n") + attributes + faces);
        FS::FileSystemManager fileSystem;
        MountAssets(fileSystem, tmp.Path());
        return ContentLoader::LoadMesh("quad.obj", fileSystem);
    }

    // Every vertex has a unit normal facing +Z (the quad's face) and a finite unit tangent.
    void CheckQuadBasis(const ContentLoader::LoadedMesh& mesh)
    {
        for (const auto& vertex : mesh.vertices)
        {
            CHECK(vertex.normal.x() == doctest::Approx(0.0f));
            CHECK(vertex.normal.y() == doctest::Approx(0.0f));
            CHECK(vertex.normal.z() == doctest::Approx(1.0f));
            const HM::Vector4& t = vertex.tangent;
            CHECK(std::isfinite(t.x()));
            CHECK(std::isfinite(t.y()));
            CHECK(std::isfinite(t.z()));
            CHECK(std::isfinite(t.w()));
            CHECK(std::sqrt(t.x() * t.x() + t.y() * t.y() + t.z() * t.z()) == doctest::Approx(1.0f));
        }
    }
}

TEST_CASE("LoadMesh - OBJ triangle loads with correct geometry")
{
    TempDir tmp;
    tmp.WriteFile("triangle.obj", TRIANGLE_OBJ);
    FS::FileSystemManager fileSystem;
    MountAssets(fileSystem, tmp.Path());

    const auto loaded = ContentLoader::LoadMesh("triangle.obj", fileSystem);
    REQUIRE(loaded.has_value());
    const ContentLoader::LoadedMesh& mesh = *loaded;

    REQUIRE(mesh.vertices.size() == 3u);
    REQUIRE(mesh.indices.size() == 3u);

    // Vertices are emitted in face-index order.
    CHECK(mesh.indices[0] == 0u);
    CHECK(mesh.indices[1] == 1u);
    CHECK(mesh.indices[2] == 2u);

    CHECK(mesh.vertices[0].position.x() == 0.0f);
    CHECK(mesh.vertices[1].position.x() == 1.0f);
    CHECK(mesh.vertices[2].position.y() == 1.0f);

    for (const auto& vertex : mesh.vertices)
    {
        CHECK(vertex.normal.x() == 0.0f);
        CHECK(vertex.normal.y() == 0.0f);
        CHECK(vertex.normal.z() == 1.0f);
    }

    // The loader flips V (1 - v).
    CHECK(mesh.vertices[0].uv.x() == 0.0f);
    CHECK(mesh.vertices[0].uv.y() == 1.0f);
}

TEST_CASE("LoadMesh - shared vertices are deduplicated")
{
    TempDir tmp;
    // Two triangles sharing the edge between vertices 2 and 3 (a quad).
    tmp.WriteFile("quad.obj",
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "v 1 1 0\n"
        "vt 0 0\n"
        "vt 1 0\n"
        "vt 0 1\n"
        "vt 1 1\n"
        "vn 0 0 1\n"
        "f 1/1/1 2/2/1 3/3/1\n"
        "f 2/2/1 4/4/1 3/3/1\n");
    FS::FileSystemManager fileSystem;
    MountAssets(fileSystem, tmp.Path());

    const auto loaded = ContentLoader::LoadMesh("quad.obj", fileSystem);
    REQUIRE(loaded.has_value());

    CHECK(loaded->vertices.size() == 4u); // 6 face corners, 4 unique vertices
    CHECK(loaded->indices.size() == 6u);
}

TEST_CASE("LoadMesh - missing file returns nullopt")
{
    TempDir tmp;
    FS::FileSystemManager fileSystem;
    MountAssets(fileSystem, tmp.Path());

    CHECK_FALSE(ContentLoader::LoadMesh("does_not_exist.obj", fileSystem).has_value());
}

TEST_CASE("LoadMesh - unsupported extension returns nullopt")
{
    TempDir tmp;
    tmp.WriteFile("not_a_mesh.txt", "hello");
    FS::FileSystemManager fileSystem;
    MountAssets(fileSystem, tmp.Path());

    CHECK_FALSE(ContentLoader::LoadMesh("not_a_mesh.txt", fileSystem).has_value());
}

TEST_CASE("LoadMesh - an OBJ with positions only gets zero UVs and generated normals")
{
    const auto mesh = LoadQuad("", "f 1 2 3\nf 1 3 4\n");
    REQUIRE(mesh.has_value());
    CHECK(mesh->vertices.size() == 4u); // shared corners still de-duplicated
    CHECK(mesh->indices.size() == 6u);
    for (const auto& vertex : mesh->vertices)
    {
        CHECK(vertex.uv.x() == 0.0f);
        CHECK(vertex.uv.y() == 0.0f);
    }
    CheckQuadBasis(*mesh);
}

TEST_CASE("LoadMesh - an OBJ with UVs and no normals gets generated normals")
{
    const auto mesh = LoadQuad("vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n", "f 1/1 2/2 3/3\nf 1/1 3/3 4/4\n");
    REQUIRE(mesh.has_value());
    CHECK(mesh->vertices.size() == 4u);
    CHECK(mesh->indices.size() == 6u);
    CHECK(mesh->vertices[1].uv.x() == 1.0f);
    CHECK(mesh->vertices[1].uv.y() == 1.0f); // v flipped
    CheckQuadBasis(*mesh);
}

TEST_CASE("LoadMesh - an OBJ with normals and no UVs gets zero UVs")
{
    const auto mesh = LoadQuad("vn 0 0 1\n", "f 1//1 2//1 3//1\nf 1//1 3//1 4//1\n");
    REQUIRE(mesh.has_value());
    CHECK(mesh->vertices.size() == 4u);
    CHECK(mesh->indices.size() == 6u);
    for (const auto& vertex : mesh->vertices)
    {
        CHECK(vertex.uv.x() == 0.0f);
        CHECK(vertex.uv.y() == 0.0f);
    }
    CheckQuadBasis(*mesh);
}
