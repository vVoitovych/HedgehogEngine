#include "doctest/doctest/doctest.h"

#include "HedgehogNavigation/api/NavigationTypes.hpp"

TEST_CASE("Recast and Detour are linked and work")
{
    CHECK(HN::CheckNavigationLibraries().empty());
}

TEST_CASE("The default build settings describe a human-sized agent within Detour's limits")
{
    const HN::NavMeshBuildSettings settings;
    CHECK(settings.CellSize > 0.0f);
    CHECK(settings.CellHeight > 0.0f);
    CHECK(settings.AgentRadius >= settings.CellSize); // at least one cell of erosion
    CHECK(settings.AgentHeight > settings.AgentMaxClimb);
    CHECK(settings.AgentMaxSlope > 0.0f);
    CHECK(settings.AgentMaxSlope < 90.0f);
    CHECK(settings.VertsPerPoly >= 3);
    CHECK(settings.VertsPerPoly <= 6);

    const HN::NavMeshGeometry geometry;
    CHECK(geometry.Positions.empty());
    CHECK(geometry.Indices.empty());
}
