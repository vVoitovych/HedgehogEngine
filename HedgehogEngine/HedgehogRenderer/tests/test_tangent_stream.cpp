#include "doctest/doctest/doctest.h"

#include "../src/ResourceRegistry/TangentStream.hpp"

#include <vector>

namespace
{
    // The geometry of one mesh, kept alive for the MeshView that refers into it.
    struct MeshData
    {
        std::vector<HM::Vector3>  Positions;
        std::vector<HM::Vector3>  Normals;
        std::vector<HM::Vector2>  TexCoords;
        std::vector<HM::Vector4>  Tangents;
        std::vector<uint32_t>     Indices;
        std::vector<HM::Vector4u> Joints;
        std::vector<HM::Vector4>  Weights;

        explicit MeshData(size_t vertexCount)
            : Positions(vertexCount, HM::Vector3(0.0f, 0.0f, 0.0f))
            , Normals(vertexCount, HM::Vector3(0.0f, 0.0f, 1.0f))
            , TexCoords(vertexCount, HM::Vector2(0.0f, 0.0f))
        {
        }

        HedgehogEngine::MeshView View() const
        {
            return { Positions, Normals, TexCoords, Tangents, Indices, Joints, Weights, 0, 0, 0 };
        }
    };
}

TEST_CASE("The tangent stream packs each vertex's tangent and handedness")
{
    MeshData mesh(2);
    mesh.Tangents = { HM::Vector4(1.0f, 0.0f, 0.0f, 1.0f), HM::Vector4(0.0f, 1.0f, 0.0f, -1.0f) };

    std::vector<float> tangents;
    HR::AppendTangentStream(mesh.View(), tangents);
    CHECK(tangents == std::vector<float>{ 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, -1.0f });
}

TEST_CASE("A mesh without a tangent per vertex gets the default, and the stream stays aligned with the positions")
{
    MeshData none(2);
    MeshData partial(3);
    partial.Tangents = { HM::Vector4(0.0f, 1.0f, 0.0f, 1.0f) }; // one for three vertices
    MeshData full(1);
    full.Tangents = { HM::Vector4(0.0f, 0.0f, 1.0f, -1.0f) };

    std::vector<float> tangents;
    HR::AppendTangentStream(none.View(), tangents);
    HR::AppendTangentStream(partial.View(), tangents);
    HR::AppendTangentStream(full.View(), tangents);

    REQUIRE(tangents.size() == (2 + 3 + 1) * 4u);
    for (size_t vertex = 0; vertex < 5; ++vertex)
    {
        CAPTURE(vertex);
        CHECK(tangents[vertex * 4 + 0] == 1.0f);
        CHECK(tangents[vertex * 4 + 1] == 0.0f);
        CHECK(tangents[vertex * 4 + 2] == 0.0f);
        CHECK(tangents[vertex * 4 + 3] == 1.0f);
    }
    // The last mesh's vertex starts where its position does: vertex 5.
    CHECK(tangents[20] == 0.0f);
    CHECK(tangents[22] == 1.0f);
    CHECK(tangents[23] == -1.0f);
}
