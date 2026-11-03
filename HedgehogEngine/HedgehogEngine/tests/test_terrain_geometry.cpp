#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/Terrain/TerrainGeometry.hpp"

#include <cmath>

using namespace HedgehogEngine;

namespace
{
    constexpr float EPSILON = 1.0e-4f;

    // A 32 m tile of 33 x 33 samples (1 m apart) whose height is the plane base + slopeX x + slopeY y.
    TerrainData MakePlane(float base, float slopeX, float slopeY)
    {
        TerrainData data = MakeDefaultTerrain();
        data.Size        = 32.0f;
        data.Resolution  = 33;
        data.Heights.resize(33 * 33);
        for (uint32_t row = 0; row < 33; ++row)
            for (uint32_t column = 0; column < 33; ++column)
                data.Heights[row * 33 + column] = base + slopeX * column + slopeY * row;
        return data;
    }

    bool Near(const HM::Vector3& a, const HM::Vector3& b, float epsilon = EPSILON)
    {
        return std::abs(a.x() - b.x()) < epsilon && std::abs(a.y() - b.y()) < epsilon && std::abs(a.z() - b.z()) < epsilon;
    }
}

TEST_CASE("A flat terrain's mesh is a grid of upward vertices and two triangles per cell")
{
    const TerrainData data = MakePlane(2.0f, 0.0f, 0.0f);
    const TerrainMesh mesh = BuildTerrainMesh(data);
    REQUIRE(mesh.Positions.size() == 33 * 33);
    CHECK(mesh.TexCoords.size() == 33 * 33);
    CHECK(mesh.Normals.size() == 33 * 33);
    CHECK(mesh.Tangents.size() == 33 * 33);
    CHECK(mesh.Indices.size() == 6 * 32 * 32);

    CHECK(Near(mesh.Positions[0], HM::Vector3(0.0f, 0.0f, 2.0f)));
    CHECK(Near(mesh.Positions[33 * 33 - 1], HM::Vector3(32.0f, 32.0f, 2.0f)));
    CHECK(Near(mesh.Positions[33 + 5], HM::Vector3(5.0f, 1.0f, 2.0f)));
    CHECK(mesh.TexCoords[33 * 33 - 1].x() == doctest::Approx(1.0f));
    CHECK(mesh.TexCoords[33 * 33 - 1].y() == doctest::Approx(1.0f));
    for (size_t i = 0; i < mesh.Normals.size(); ++i)
    {
        CHECK(Near(mesh.Normals[i], HM::Vector3(0.0f, 0.0f, 1.0f)));
        CHECK(mesh.Tangents[i].x() == doctest::Approx(1.0f));
        CHECK(mesh.Tangents[i].w() == 1.0f);
    }

    // Every triangle faces up (counterclockwise from above).
    for (size_t i = 0; i < mesh.Indices.size(); i += 3)
    {
        const HM::Vector3 a = mesh.Positions[mesh.Indices[i]];
        const HM::Vector3 b = mesh.Positions[mesh.Indices[i + 1]];
        const HM::Vector3 c = mesh.Positions[mesh.Indices[i + 2]];
        CHECK(HM::Cross(b - a, c - a).z() > 0.0f);
    }
}

TEST_CASE("A sloped terrain's normals and tangents follow the slope, and its height is the plane")
{
    const TerrainData data = MakePlane(1.0f, 0.5f, -0.25f);
    const TerrainMesh mesh = BuildTerrainMesh(data);
    const HM::Vector3 normal = HM::Vector3(-0.5f, 0.25f, 1.0f).Normalize();
    for (size_t i = 0; i < mesh.Normals.size(); ++i)
    {
        CHECK(Near(mesh.Normals[i], normal));
        const HM::Vector3 tangent(mesh.Tangents[i].x(), mesh.Tangents[i].y(), mesh.Tangents[i].z());
        CHECK(std::abs(HM::Dot(tangent, mesh.Normals[i])) < EPSILON);
        CHECK(tangent.LengthSlow() == doctest::Approx(1.0f));
        CHECK(tangent.x() > 0.0f);
    }

    CHECK(SampleTerrainHeight(data, 0.0f, 0.0f) == doctest::Approx(1.0f));
    CHECK(SampleTerrainHeight(data, 10.3f, 4.7f) == doctest::Approx(1.0f + 0.5f * 10.3f - 0.25f * 4.7f));
    CHECK(SampleTerrainHeight(data, 7.8f, 7.2f) == doctest::Approx(1.0f + 0.5f * 7.8f - 0.25f * 7.2f));
    // Clamped at the edges.
    CHECK(SampleTerrainHeight(data, -5.0f, 40.0f) == doctest::Approx(1.0f - 0.25f * 32.0f));
}

TEST_CASE("The height inside a cell follows its two triangles")
{
    TerrainData data = MakePlane(0.0f, 0.0f, 0.0f);
    data.Heights[0 * 33 + 1] = 4.0f; // (1, 0)
    // On the (0, 0)-(1, 0)-(1, 1) triangle the height is 4 at (1, 0) and 0 on the diagonal.
    CHECK(SampleTerrainHeight(data, 0.75f, 0.25f) == doctest::Approx(4.0f * 0.5f));
    CHECK(SampleTerrainHeight(data, 0.5f, 0.5f) == doctest::Approx(0.0f));
    // The other triangle does not see it.
    CHECK(SampleTerrainHeight(data, 0.25f, 0.75f) == doctest::Approx(0.0f));
}

TEST_CASE("Rays hit a terrain at the surface's analytic point")
{
    const TerrainData flat = MakePlane(3.0f, 0.0f, 0.0f);
    std::optional<TerrainRayHit> hit = RaycastTerrain(flat, HM::Vector3(12.5f, 7.25f, 20.0f), HM::Vector3(0.0f, 0.0f, -2.0f), 100.0f);
    REQUIRE(hit);
    CHECK(Near(hit->Point, HM::Vector3(12.5f, 7.25f, 3.0f)));
    CHECK(Near(hit->Normal, HM::Vector3(0.0f, 0.0f, 1.0f)));
    CHECK(hit->Distance == doctest::Approx(17.0f));

    // A slanted ray from outside the tile, crossing many cells.
    const HM::Vector3 origin(-10.0f, -5.0f, 13.0f);
    const HM::Vector3 direction = HM::Vector3(1.0f, 0.5f, -0.4f);
    hit = RaycastTerrain(flat, origin, direction, 100.0f);
    REQUIRE(hit);
    const float t = (13.0f - 3.0f) / 0.4f; // along the unnormalized direction
    CHECK(Near(hit->Point, origin + direction * t, 1.0e-3f));

    const TerrainData sloped = MakePlane(1.0f, 0.5f, -0.25f);
    hit = RaycastTerrain(sloped, HM::Vector3(20.2f, 10.6f, 50.0f), HM::Vector3(0.0f, 0.0f, -1.0f), 100.0f);
    REQUIRE(hit);
    CHECK(hit->Point.z() == doctest::Approx(SampleTerrainHeight(sloped, 20.2f, 10.6f)));
    CHECK(Near(hit->Normal, HM::Vector3(-0.5f, 0.25f, 1.0f).Normalize()));

    // From below too.
    hit = RaycastTerrain(flat, HM::Vector3(4.0f, 4.0f, -1.0f), HM::Vector3(0.0f, 0.0f, 1.0f), 10.0f);
    REQUIRE(hit);
    CHECK(hit->Point.z() == doctest::Approx(3.0f));
}

TEST_CASE("Rays that miss a terrain hit nothing")
{
    const TerrainData flat = MakePlane(3.0f, 0.0f, 0.0f);
    // Outside the tile, parallel above it, pointing away, past the distance, a zero direction.
    CHECK_FALSE(RaycastTerrain(flat, HM::Vector3(40.0f, 4.0f, 10.0f), HM::Vector3(0.0f, 0.0f, -1.0f), 100.0f));
    CHECK_FALSE(RaycastTerrain(flat, HM::Vector3(-1.0f, 4.0f, 5.0f), HM::Vector3(1.0f, 0.0f, 0.0f), 100.0f));
    CHECK_FALSE(RaycastTerrain(flat, HM::Vector3(4.0f, 4.0f, 10.0f), HM::Vector3(0.0f, 0.0f, 1.0f), 100.0f));
    CHECK_FALSE(RaycastTerrain(flat, HM::Vector3(4.0f, 4.0f, 10.0f), HM::Vector3(0.0f, 0.0f, -1.0f), 6.0f));
    CHECK_FALSE(RaycastTerrain(flat, HM::Vector3(4.0f, 4.0f, 10.0f), HM::Vector3(0.0f, 0.0f, 0.0f), 100.0f));
    CHECK_FALSE(RaycastTerrain(flat, HM::Vector3(4.0f, 4.0f, 10.0f), HM::Vector3(0.0f, 0.0f, -1.0f), 0.0f));
}

TEST_CASE("A ray grazing a hill hits it where it rises, not the flat ground beyond")
{
    TerrainData data = MakePlane(0.0f, 0.0f, 0.0f);
    data.Heights[16 * 33 + 16] = 10.0f; // a spike at (16, 16)
    const std::optional<TerrainRayHit> hit =
        RaycastTerrain(data, HM::Vector3(0.0f, 16.0f, 5.0f), HM::Vector3(1.0f, 0.0f, 0.0f), 100.0f);
    REQUIRE(hit);
    CHECK(hit->Point.x() > 15.0f);
    CHECK(hit->Point.x() < 16.0f);
    CHECK(hit->Point.z() == doctest::Approx(5.0f));
}
