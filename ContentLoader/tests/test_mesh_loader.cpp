#include "doctest/doctest/doctest.h"

#include "ContentLoader/api/MeshLoader.hpp"
#include "ContentLoader/api/LoadedData.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"

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
}

TEST_CASE("LoadMesh - OBJ triangle loads with correct geometry")
{
    TempDir tmp;
    tmp.WriteFile("triangle.obj", TRIANGLE_OBJ);
    FS::FileSystemManager fileSystem;
    MountAssets(fileSystem, tmp.Path());

    const ContentLoader::LoadedMesh mesh = ContentLoader::LoadMesh("triangle.obj", fileSystem);

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

    const ContentLoader::LoadedMesh mesh = ContentLoader::LoadMesh("quad.obj", fileSystem);

    CHECK(mesh.vertices.size() == 4u); // 6 face corners, 4 unique vertices
    CHECK(mesh.indices.size() == 6u);
}

TEST_CASE("LoadMesh - missing file throws")
{
    TempDir tmp;
    FS::FileSystemManager fileSystem;
    MountAssets(fileSystem, tmp.Path());

    CHECK_THROWS(ContentLoader::LoadMesh("does_not_exist.obj", fileSystem));
}

TEST_CASE("LoadMesh - unsupported extension throws")
{
    TempDir tmp;
    tmp.WriteFile("not_a_mesh.txt", "hello");
    FS::FileSystemManager fileSystem;
    MountAssets(fileSystem, tmp.Path());

    CHECK_THROWS(ContentLoader::LoadMesh("not_a_mesh.txt", fileSystem));
}
