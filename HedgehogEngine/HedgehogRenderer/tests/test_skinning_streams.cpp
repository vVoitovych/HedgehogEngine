#include "doctest/doctest/doctest.h"

#include "../src/ResourceRegistry/SkinningStreams.hpp"

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
            , Normals(vertexCount, HM::Vector3(0.0f, 1.0f, 0.0f))
            , TexCoords(vertexCount, HM::Vector2(0.0f, 0.0f))
        {
        }

        HedgehogEngine::MeshView View() const
        {
            return { Positions, Normals, TexCoords, Tangents, Indices, Joints, Weights, 0, 0, 0 };
        }
    };
}

TEST_CASE("Skinning streams pack a skinned mesh's joints and weights four per vertex")
{
    MeshData mesh(2);
    mesh.Joints  = { HM::Vector4u(0, 1, 2, 3), HM::Vector4u(4, 5, 6, 7) };
    mesh.Weights = { HM::Vector4(0.5f, 0.25f, 0.25f, 0.0f), HM::Vector4(1.0f, 0.0f, 0.0f, 0.0f) };

    std::vector<uint32_t> joints;
    std::vector<float>    weights;
    HR::AppendSkinningStreams(mesh.View(), joints, weights);

    CHECK(joints == std::vector<uint32_t>{ 0, 1, 2, 3, 4, 5, 6, 7 });
    CHECK(weights == std::vector<float>{ 0.5f, 0.25f, 0.25f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f });
}

TEST_CASE("Skinning streams are zero for a static mesh and stay aligned with the positions after it")
{
    MeshData staticMesh(3);
    MeshData skinned(1);
    skinned.Joints  = { HM::Vector4u(9, 0, 0, 0) };
    skinned.Weights = { HM::Vector4(1.0f, 0.0f, 0.0f, 0.0f) };

    std::vector<uint32_t> joints;
    std::vector<float>    weights;
    HR::AppendSkinningStreams(staticMesh.View(), joints, weights);
    HR::AppendSkinningStreams(skinned.View(), joints, weights);

    REQUIRE(joints.size() == 16u);
    REQUIRE(weights.size() == 16u);
    for (size_t i = 0; i < 12; ++i)
    {
        CHECK(joints[i] == 0u);
        CHECK(weights[i] == 0.0f);
    }
    // The skinned mesh's vertex starts where its position does: vertex 3.
    CHECK(joints[12] == 9u);
    CHECK(weights[12] == 1.0f);
}

TEST_CASE("Skinning streams are zero when the joints or weights do not cover every vertex")
{
    MeshData mesh(2);
    mesh.Joints  = { HM::Vector4u(1, 1, 1, 1) };
    mesh.Weights = { HM::Vector4(1.0f, 0.0f, 0.0f, 0.0f) };

    std::vector<uint32_t> joints;
    std::vector<float>    weights;
    HR::AppendSkinningStreams(mesh.View(), joints, weights);

    CHECK(joints == std::vector<uint32_t>(8, 0u));
    CHECK(weights == std::vector<float>(8, 0.0f));
}
