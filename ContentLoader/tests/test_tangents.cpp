#include "doctest/doctest/doctest.h"

#include "ContentLoader/api/LoadedData.hpp"
#include "ContentLoader/api/MeshLoader.hpp"
#include "ContentLoader/api/Tangents.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"
#include "HedgehogScripting/tests/test_log_capture.hpp"

#include "test_gltf_builder.hpp"

#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

using ContentLoader::LoadedMesh;
using ContentLoader::LoadedVertexData;
using namespace GltfTest;

namespace
{
    bool Near(float a, float b, float epsilon = 1e-5f) { return std::abs(a - b) < epsilon; }

    bool Near(const HM::Vector4& a, const HM::Vector4& b)
    {
        return Near(a.x(), b.x()) && Near(a.y(), b.y()) && Near(a.z(), b.z()) && Near(a.w(), b.w());
    }

    LoadedVertexData Vertex(float x, float y, float u, float v, const HM::Vector3& normal = HM::Vector3(0.0f, 0.0f, 1.0f))
    {
        LoadedVertexData vertex;
        vertex.position = HM::Vector3(x, y, 0.0f);
        vertex.uv       = HM::Vector2(u, v);
        vertex.normal   = normal;
        vertex.tangent  = HM::Vector4(0.0f, 0.0f, 0.0f, 0.0f); // so a vertex the generator skips shows
        return vertex;
    }

    // A unit quad in the XY plane facing +Z, as two triangles.
    const std::vector<uint32_t> QUAD = { 0, 1, 2, 0, 2, 3 };

    // A unit tangent orthogonal to the normal, with a handedness of +1 or -1.
    void CheckBasis(const LoadedVertexData& vertex)
    {
        const HM::Vector4& t = vertex.tangent;
        const HM::Vector3& n = vertex.normal;
        const float        nLength = std::sqrt(n.x() * n.x() + n.y() * n.y() + n.z() * n.z());
        CHECK(Near(t.x() * t.x() + t.y() * t.y() + t.z() * t.z(), 1.0f, 1e-4f));
        CHECK(std::abs(t.x() * n.x() + t.y() * n.y() + t.z() * n.z()) < 1e-3f * nLength);
        CHECK((t.w() == 1.0f || t.w() == -1.0f));
    }

    // One quad as a glTF mesh; with tangents, every vertex's TANGENT is given (w -1, to tell it from
    // a generated one). twoPrimitives adds a second copy as another primitive, with tangents only when
    // secondHasTangents.
    std::string QuadGltf(bool tangents, bool twoPrimitives = false, bool secondHasTangents = false)
    {
        GltfBuilder builder;
        const int position = builder.AddAccessor(
            builder.AddView(Bytes(std::vector<float>{ 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0 })), FLOAT, 4, "VEC3");
        const int normal = builder.AddAccessor(
            builder.AddView(Bytes(std::vector<float>{ 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1 })), FLOAT, 4, "VEC3");
        const int uv = builder.AddAccessor(builder.AddView(Bytes(std::vector<float>{ 0, 0, 1, 0, 1, 1, 0, 1 })), FLOAT,
                                           4, "VEC2");
        const int tangent = builder.AddAccessor(
            builder.AddView(Bytes(std::vector<float>{ 0, 1, 0, -1, 0, 1, 0, -1, 0, 1, 0, -1, 0, 1, 0, -1 })), FLOAT, 4,
            "VEC4");
        const int indices = builder.AddAccessor(builder.AddView(Bytes(QUAD)), 5125, 6, "SCALAR");

        const auto primitive = [&](bool withTangent)
        {
            return "{\"attributes\":{\"POSITION\":" + std::to_string(position) + ",\"NORMAL\":" + std::to_string(normal) +
                   ",\"TEXCOORD_0\":" + std::to_string(uv) +
                   (withTangent ? ",\"TANGENT\":" + std::to_string(tangent) : std::string()) +
                   "},\"indices\":" + std::to_string(indices) + "}";
        };
        std::string primitives = primitive(tangents);
        if (twoPrimitives)
            primitives += "," + primitive(secondHasTangents);
        return builder.Gltf("\"meshes\":[{\"primitives\":[" + primitives +
                            "]}],\"nodes\":[{\"mesh\":0}],\"scenes\":[{\"nodes\":[0]}],\"scene\":0");
    }

    std::optional<LoadedMesh> LoadFile(const std::string& name, const std::string& content)
    {
        TempDir tmp;
        tmp.WriteFile(name, content);
        FS::FileSystemManager fileSystem;
        MountAssets(fileSystem, tmp.Path());
        return ContentLoader::LoadMesh(name, fileSystem);
    }
}

TEST_CASE("Tangents - a quad's tangent runs along increasing u, with the bitangent along increasing v")
{
    std::vector<LoadedVertexData> vertices = { Vertex(0, 0, 0, 0), Vertex(1, 0, 1, 0), Vertex(1, 1, 1, 1), Vertex(0, 1, 0, 1) };
    ContentLoader::GenerateTangents(vertices, QUAD);
    for (const LoadedVertexData& vertex : vertices)
    {
        CHECK(Near(vertex.tangent, HM::Vector4(1.0f, 0.0f, 0.0f, 1.0f)));
        CheckBasis(vertex);
    }

    // The texture turned a quarter: u now runs along +Y and v along -X.
    vertices = { Vertex(0, 0, 0, 1), Vertex(1, 0, 0, 0), Vertex(1, 1, 1, 0), Vertex(0, 1, 1, 1) };
    ContentLoader::GenerateTangents(vertices, QUAD);
    for (const LoadedVertexData& vertex : vertices)
        CHECK(Near(vertex.tangent, HM::Vector4(0.0f, 1.0f, 0.0f, 1.0f)));
}

TEST_CASE("Tangents - mirrored UVs give a handedness of -1")
{
    // u runs along -X while v still runs along +Y: cross(normal, tangent) points down v.
    std::vector<LoadedVertexData> vertices = { Vertex(0, 0, 1, 0), Vertex(1, 0, 0, 0), Vertex(1, 1, 0, 1), Vertex(0, 1, 1, 1) };
    ContentLoader::GenerateTangents(vertices, QUAD);
    for (const LoadedVertexData& vertex : vertices)
    {
        CHECK(Near(vertex.tangent, HM::Vector4(-1.0f, 0.0f, 0.0f, -1.0f)));
        CheckBasis(vertex);
    }
}

TEST_CASE("Tangents - a tangent is made orthogonal to a normal that is not the face's")
{
    // The normals lean towards +X, so the face's tangent (+X) cannot stay as it is.
    const HM::Vector3             leaning(0.6f, 0.0f, 0.8f);
    std::vector<LoadedVertexData> vertices = { Vertex(0, 0, 0, 0, leaning), Vertex(1, 0, 1, 0, leaning),
                                               Vertex(1, 1, 1, 1, leaning), Vertex(0, 1, 0, 1, leaning) };
    ContentLoader::GenerateTangents(vertices, QUAD);
    for (const LoadedVertexData& vertex : vertices)
    {
        CheckBasis(vertex);
        CHECK(Near(vertex.tangent, HM::Vector4(0.8f, 0.0f, -0.6f, 1.0f)));
    }
}

TEST_CASE("Tangents - degenerate UVs, no normal and stray indices still give a usable basis")
{
    SUBCASE("every UV the same")
    {
        std::vector<LoadedVertexData> vertices = { Vertex(0, 0, 0.5f, 0.5f), Vertex(1, 0, 0.5f, 0.5f),
                                                   Vertex(1, 1, 0.5f, 0.5f), Vertex(0, 1, 0.5f, 0.5f) };
        ContentLoader::GenerateTangents(vertices, QUAD);
        for (const LoadedVertexData& vertex : vertices)
        {
            CheckBasis(vertex);
            CHECK(vertex.tangent.w() == 1.0f);
        }
    }
    SUBCASE("UVs along a line")
    {
        std::vector<LoadedVertexData> vertices = { Vertex(0, 0, 0, 0), Vertex(1, 0, 1, 0), Vertex(1, 1, 2, 0), Vertex(0, 1, 1, 0) };
        ContentLoader::GenerateTangents(vertices, QUAD);
        for (const LoadedVertexData& vertex : vertices)
            CheckBasis(vertex);
    }
    SUBCASE("no normal")
    {
        const HM::Vector3             none(0.0f, 0.0f, 0.0f);
        std::vector<LoadedVertexData> vertices = { Vertex(0, 0, 0, 0, none), Vertex(1, 0, 1, 0, none), Vertex(1, 1, 1, 1, none) };
        ContentLoader::GenerateTangents(vertices, { 0, 1, 2 });
        for (const LoadedVertexData& vertex : vertices)
            CHECK(Near(vertex.tangent, HM::Vector4(1.0f, 0.0f, 0.0f, 1.0f)));
    }
    SUBCASE("an index out of range and a trailing partial triangle")
    {
        std::vector<LoadedVertexData> vertices = { Vertex(0, 0, 0, 0), Vertex(1, 0, 1, 0), Vertex(1, 1, 1, 1), Vertex(0, 1, 0, 1) };
        ContentLoader::GenerateTangents(vertices, { 0, 1, 2, 0, 2, 9, 3, 0 });
        for (const LoadedVertexData& vertex : vertices)
            CheckBasis(vertex);
        // Vertex 3 is in no whole triangle, so it gets a basis without UVs.
        CHECK(Near(vertices[0].tangent, HM::Vector4(1.0f, 0.0f, 0.0f, 1.0f)));
    }
}

TEST_CASE("Tangents - a glTF TANGENT attribute is used as written, and a file without one is generated")
{
    LogCapture log;
    const std::optional<LoadedMesh> given = LoadFile("Given.gltf", QuadGltf(true));
    REQUIRE(given.has_value());
    REQUIRE(given->vertices.size() == 4);
    for (const LoadedVertexData& vertex : given->vertices)
        CHECK(Near(vertex.tangent, HM::Vector4(0.0f, 1.0f, 0.0f, -1.0f)));

    const std::optional<LoadedMesh> generated = LoadFile("Generated.gltf", QuadGltf(false));
    REQUIRE(generated.has_value());
    for (const LoadedVertexData& vertex : generated->vertices)
        CHECK(Near(vertex.tangent, HM::Vector4(1.0f, 0.0f, 0.0f, 1.0f)));

    // One primitive without TANGENT makes the whole mesh generated.
    const std::optional<LoadedMesh> mixed = LoadFile("Mixed.gltf", QuadGltf(true, true, false));
    REQUIRE(mixed.has_value());
    REQUIRE(mixed->vertices.size() == 8);
    for (const LoadedVertexData& vertex : mixed->vertices)
        CHECK(Near(vertex.tangent, HM::Vector4(1.0f, 0.0f, 0.0f, 1.0f)));
    const std::optional<LoadedMesh> both = LoadFile("Both.gltf", QuadGltf(true, true, true));
    REQUIRE(both.has_value());
    for (const LoadedVertexData& vertex : both->vertices)
        CHECK(vertex.tangent.w() == -1.0f);
    CHECK(log.Lines("[WARNING]").empty());
    CHECK(log.Lines("[ERROR]").empty());
}

TEST_CASE("Tangents - DamagedHelmet, which has no TANGENT, loads with generated tangents and no warning")
{
    // tests/ -> ContentLoader/ -> repository root.
    const std::filesystem::path assets =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Projects/FeatureTest/Assets";
    FS::FileSystemManager fileSystem;
    MountAssets(fileSystem, assets);

    LogCapture                      log;
    const std::optional<LoadedMesh> helmet = ContentLoader::LoadMesh("Models/DamagedHelmet/DamagedHelmet.gltf", fileSystem);
    REQUIRE(helmet.has_value());
    REQUIRE(!helmet->vertices.empty());
    size_t mirrored = 0;
    for (const LoadedVertexData& vertex : helmet->vertices)
    {
        CheckBasis(vertex);
        mirrored += vertex.tangent.w() < 0.0f ? 1 : 0;
    }
    // Its UVs are not mirrored everywhere: most vertices are right-handed.
    CHECK(mirrored < helmet->vertices.size());
    CHECK(log.Lines("[WARNING]").empty());
    CHECK(log.Lines("[ERROR]").empty());
}

TEST_CASE("Tangents - an OBJ's vertices get generated tangents")
{
    const std::optional<LoadedMesh> quad = LoadFile("Quad.obj",
                                                    "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
                                                    "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nvn 0 0 1\n"
                                                    "f 1/1/1 2/2/1 3/3/1\nf 1/1/1 3/3/1 4/4/1\n");
    REQUIRE(quad.has_value());
    REQUIRE(quad->vertices.size() == 4);
    // The OBJ loader flips v (1 - v), so the bitangent runs down the file's v: left-handed.
    for (const LoadedVertexData& vertex : quad->vertices)
    {
        CheckBasis(vertex);
        CHECK(Near(vertex.tangent, HM::Vector4(1.0f, 0.0f, 0.0f, -1.0f)));
    }
}
