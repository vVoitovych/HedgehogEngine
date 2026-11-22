#include "doctest/doctest/doctest.h"

#include "HedgehogNavigation/api/NavMeshBake.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <set>

using HN::NavMeshGeometry;

namespace
{
    // A quad of four corners, counterclockwise seen from its walkable side.
    void AddQuad(NavMeshGeometry& g, const HM::Vector3& a, const HM::Vector3& b, const HM::Vector3& c, const HM::Vector3& d)
    {
        const auto first = static_cast<uint32_t>(g.Positions.size());
        g.Positions.insert(g.Positions.end(), { a, b, c, d });
        g.Indices.insert(g.Indices.end(), { first, first + 1, first + 2, first, first + 2, first + 3 });
    }

    // A horizontal rectangle at height z, facing up.
    void AddFloor(NavMeshGeometry& g, float x0, float y0, float x1, float y1, float z)
    {
        AddQuad(g, HM::Vector3(x0, y0, z), HM::Vector3(x1, y0, z), HM::Vector3(x1, y1, z), HM::Vector3(x0, y1, z));
    }

    // A closed box from (x0, y0, 0) to (x1, y1, h): its top and four sides.
    void AddBox(NavMeshGeometry& g, float x0, float y0, float x1, float y1, float h)
    {
        AddFloor(g, x0, y0, x1, y1, h);
        AddQuad(g, HM::Vector3(x0, y0, 0.0f), HM::Vector3(x0, y0, h), HM::Vector3(x1, y0, h), HM::Vector3(x1, y0, 0.0f));
        AddQuad(g, HM::Vector3(x1, y0, 0.0f), HM::Vector3(x1, y0, h), HM::Vector3(x1, y1, h), HM::Vector3(x1, y1, 0.0f));
        AddQuad(g, HM::Vector3(x1, y1, 0.0f), HM::Vector3(x1, y1, h), HM::Vector3(x0, y1, h), HM::Vector3(x0, y1, 0.0f));
        AddQuad(g, HM::Vector3(x0, y1, 0.0f), HM::Vector3(x0, y1, h), HM::Vector3(x0, y0, h), HM::Vector3(x0, y0, 0.0f));
    }

    // Flat ground over x 0..10 and a ramp rising along +x from x 10 to 20 at degrees.
    NavMeshGeometry MakeRamp(float degrees)
    {
        NavMeshGeometry g;
        AddFloor(g, 0, 0, 10, 10, 0);
        const float top = 10.0f * std::tan(degrees * std::numbers::pi_v<float> / 180.0f);
        AddQuad(g, HM::Vector3(10.0f, 0.0f, 0.0f), HM::Vector3(20.0f, 0.0f, top), HM::Vector3(20.0f, 10.0f, top), HM::Vector3(10.0f, 10.0f, 0.0f));
        return g;
    }

    HN::NavMeshDescription Describe(const HN::NavMeshBakeResult& result)
    {
        REQUIRE(result.Error.empty());
        const auto description = HN::DescribeNavMesh(result.Data);
        REQUIRE(description.has_value());
        return *description;
    }
}

TEST_CASE("A 20 x 20 m plane bakes into a navmesh Detour accepts")
{
    NavMeshGeometry plane;
    AddFloor(plane, -10, -10, 10, 10, 0);
    const HN::NavMeshBakeResult result = HN::BakeNavMesh(plane, {});
    REQUIRE(result.Error.empty());
    CHECK(result.PolygonCount >= 1);
    CHECK_FALSE(result.Data.empty());

    const HN::NavMeshDescription description = Describe(result);
    CHECK(description.PolygonCentres.size() == result.PolygonCount);
    CHECK(description.IslandCount == 1);
    for (const HM::Vector3& centre : description.PolygonCentres)
    {
        // Z-up again, on the plane and inside it, eroded by the agent's radius.
        CHECK(std::abs(centre.z()) < 0.3f);
        CHECK(std::abs(centre.x()) < 10.0f);
        CHECK(std::abs(centre.y()) < 10.0f);
    }
}

TEST_CASE("A box on the plane leaves a hole the agent's radius wide")
{
    NavMeshGeometry g;
    AddFloor(g, -10, -10, 10, 10, 0);
    AddBox(g, -1, -1, 1, 1, 2);
    const HN::NavMeshBuildSettings settings;
    const HN::NavMeshDescription   description = Describe(HN::BakeNavMesh(g, settings));

    size_t ground = 0;
    for (const HM::Vector3& centre : description.PolygonCentres)
    {
        if (centre.z() > 1.0f)
            continue; // the box's top, if a region survives there
        ++ground;
        const float dx = std::max(0.0f, std::abs(centre.x()) - 1.0f);
        const float dy = std::max(0.0f, std::abs(centre.y()) - 1.0f);
        CHECK(std::sqrt(dx * dx + dy * dy) >= settings.AgentRadius);
    }
    CHECK(ground >= 4); // the ground around the box is several polygons
}

TEST_CASE("A ramp steeper than the walkable slope is not walked, a gentler one is")
{
    const auto onRamp = [](const HN::NavMeshDescription& d)
    {
        return std::count_if(d.PolygonCentres.begin(), d.PolygonCentres.end(),
                             [](const HM::Vector3& c) { return c.x() > 11.0f && c.z() > 0.5f; });
    };
    HN::NavMeshBuildSettings settings;
    settings.AgentMaxSlope = 45.0f;

    const HN::NavMeshDescription steep = Describe(HN::BakeNavMesh(MakeRamp(60.0f), settings));
    CHECK(onRamp(steep) == 0);

    const HN::NavMeshDescription gentle = Describe(HN::BakeNavMesh(MakeRamp(30.0f), settings));
    CHECK(onRamp(gentle) > 0);
    CHECK(gentle.IslandCount == 1); // the ramp joins the ground
}

TEST_CASE("A step higher than the agent climbs separates the floors; a lower one does not")
{
    const auto islandsOf = [](float stepHeight)
    {
        NavMeshGeometry g;
        AddFloor(g, 0, 0, 10, 10, 0);
        AddFloor(g, 10, 0, 20, 10, stepHeight);
        AddQuad(g, HM::Vector3(10.0f, 0.0f, 0.0f), HM::Vector3(10.0f, 10.0f, 0.0f), HM::Vector3(10.0f, 10.0f, stepHeight), HM::Vector3(10.0f, 0.0f, stepHeight));
        const HN::NavMeshDescription d = Describe(HN::BakeNavMesh(g, {}));
        std::set<uint32_t> low;
        std::set<uint32_t> high;
        for (size_t i = 0; i < d.PolygonCentres.size(); ++i)
            (d.PolygonCentres[i].x() < 10.0f ? low : high).insert(d.Islands[i]);
        REQUIRE_FALSE(low.empty());
        REQUIRE_FALSE(high.empty());
        return std::make_pair(low, high);
    };

    HN::NavMeshBuildSettings settings;
    REQUIRE(settings.AgentMaxClimb < 1.0f);
    const auto [lowA, highA] = islandsOf(1.0f);
    for (uint32_t island : lowA)
        CHECK_FALSE(highA.contains(island));

    REQUIRE(settings.AgentMaxClimb > 0.2f);
    const auto [lowB, highB] = islandsOf(0.2f);
    CHECK(lowB == highB);
}

TEST_CASE("The same input bakes the same bytes")
{
    NavMeshGeometry g;
    AddFloor(g, -10, -10, 10, 10, 0);
    AddBox(g, 2, 2, 4, 5, 2);
    const HN::NavMeshBakeResult a = HN::BakeNavMesh(g, {});
    const HN::NavMeshBakeResult b = HN::BakeNavMesh(g, {});
    REQUIRE(a.Error.empty());
    CHECK(a.Data == b.Data);
    CHECK(a.PolygonCount == b.PolygonCount);
}

TEST_CASE("Input that cannot be baked is refused, naming why, with no data")
{
    NavMeshGeometry plane;
    AddFloor(plane, 0, 0, 10, 10, 0);

    const auto refused = [](const NavMeshGeometry& g, const HN::NavMeshBuildSettings& s, const char* reason)
    {
        CAPTURE(reason);
        const HN::NavMeshBakeResult result = HN::BakeNavMesh(g, s);
        CHECK(result.Data.empty());
        CHECK(result.PolygonCount == 0);
        CHECK(result.Error.find(reason) != std::string::npos);
    };

    refused({}, {}, "no triangles");

    NavMeshGeometry partial = plane;
    partial.Indices.pop_back();
    refused(partial, {}, "not a whole number of triangles");

    NavMeshGeometry outOfRange = plane;
    outOfRange.Indices[4] = 99;
    refused(outOfRange, {}, "is past the 4 positions");

    NavMeshGeometry nan = plane;
    nan.Positions[2] = HM::Vector3(std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f);
    refused(nan, {}, "position 2 is not finite");

    HN::NavMeshBuildSettings noCell;
    noCell.CellSize = 0.0f;
    refused(plane, noCell, "settings are out of range");
}
