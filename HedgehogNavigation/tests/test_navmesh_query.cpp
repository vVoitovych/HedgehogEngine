#include "doctest/doctest/doctest.h"

#include "HedgehogNavigation/api/NavMesh.hpp"
#include "HedgehogNavigation/api/NavMeshBake.hpp"

#include "HedgehogScripting/tests/test_log_capture.hpp"

#include "test_geometry.hpp"

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <new>
#include <vector>

using namespace NavTest;

// Counts global allocations while s_Counting is set, so a test can prove queries allocate nothing.
// This test executable owns the global operator new.
namespace
{
    std::atomic<bool>   s_Counting    = false;
    std::atomic<size_t> s_Allocations = 0;
}

void* operator new(size_t size)
{
    if (s_Counting)
        ++s_Allocations;
    if (void* memory = std::malloc(size == 0 ? 1 : size))
        return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept
{
    std::free(memory);
}

void operator delete(void* memory, size_t) noexcept
{
    std::free(memory);
}

namespace
{
    constexpr float GAP_MIN_Y = 2.0f;
    constexpr float GAP_MAX_Y = 6.0f;

    // Ground from x -15 to 15 and y -10 to 10, split along x = 0 by a 3 m wall with a gap from
    // y 2 to 6, and an island floor beyond x 20 no one can reach.
    HN::NavMesh MakeWalledNavMesh()
    {
        NavMeshGeometry g;
        AddFloor(g, -15, -10, 15, 10, 0);
        AddBox(g, -0.3f, -10, 0.3f, GAP_MIN_Y, 3);
        AddBox(g, -0.3f, GAP_MAX_Y, 0.3f, 10, 3);
        AddFloor(g, 20, -5, 25, 5, 0);
        const HN::NavMeshBakeResult baked = HN::BakeNavMesh(g, {});
        REQUIRE(baked.Error.empty());
        HN::NavMesh mesh;
        REQUIRE(mesh.Load(baked.Data));
        return mesh;
    }

    bool NearMesh(HN::NavMesh& mesh, const HM::Vector3& point)
    {
        const auto snapped = mesh.SamplePosition(point, 0.3f);
        return snapped && (*snapped - point).LengthSlow() < 0.3f;
    }
}

TEST_CASE("A path through a wall goes through its gap, every corner on the mesh")
{
    HN::NavMesh              mesh = MakeWalledNavMesh();
    std::vector<HM::Vector3> corners;
    const HM::Vector3        start(-10.0f, -5.0f, 0.0f);
    const HM::Vector3        end(10.0f, -5.0f, 0.0f);
    REQUIRE(mesh.FindPath(start, end, corners) == HN::NavPathStatus::Complete);
    REQUIRE(corners.size() >= 3); // around the wall's end at least once

    CHECK((corners.front() - start).LengthSlow() < 0.3f);
    CHECK((corners.back() - end).LengthSlow() < 0.3f);
    for (const HM::Vector3& corner : corners)
        CHECK(NearMesh(mesh, corner));
    // Every segment crossing the wall's plane does so in the gap.
    for (size_t i = 1; i < corners.size(); ++i)
    {
        const HM::Vector3& a = corners[i - 1];
        const HM::Vector3& b = corners[i];
        if ((a.x() < 0.0f) == (b.x() < 0.0f))
            continue;
        const float t = a.x() / (a.x() - b.x());
        const float y = a.y() + (b.y() - a.y()) * t;
        CHECK(y > GAP_MIN_Y);
        CHECK(y < GAP_MAX_Y);
    }
}

TEST_CASE("A start off the mesh snaps onto it; an unreachable end gives a partial path")
{
    HN::NavMesh              mesh = MakeWalledNavMesh();
    std::vector<HM::Vector3> corners;

    REQUIRE(mesh.FindPath(HM::Vector3(-10.0f, -5.0f, 1.0f), HM::Vector3(-5.0f, -5.0f, 0.0f), corners) ==
            HN::NavPathStatus::Complete);
    CHECK(std::abs(corners.front().z()) < 0.3f); // snapped down onto the ground

    // The island is beyond a 5 m gap: the path stops at the main floor's edge, short of it.
    REQUIRE(mesh.FindPath(HM::Vector3(-10.0f, 0.0f, 0.0f), HM::Vector3(22.0f, 0.0f, 0.0f), corners) ==
            HN::NavPathStatus::Partial);
    CHECK(corners.back().x() < 15.0f);
    CHECK(corners.back().x() > 13.0f);

    // An end nowhere near the mesh, and a mesh not loaded, give no path.
    CHECK(mesh.FindPath(HM::Vector3(-10.0f, 0.0f, 0.0f), HM::Vector3(100.0f, 100.0f, 0.0f), corners) == HN::NavPathStatus::None);
    CHECK(corners.empty());
    HN::NavMesh empty;
    CHECK(empty.FindPath(HM::Vector3(0.0f, 0.0f, 0.0f), HM::Vector3(1.0f, 0.0f, 0.0f), corners) == HN::NavPathStatus::None);
}

TEST_CASE("SamplePosition finds the nearest point on the mesh within its distance")
{
    HN::NavMesh mesh = MakeWalledNavMesh();
    const auto  below = mesh.SamplePosition(HM::Vector3(5.0f, 2.0f, 1.5f), 2.0f);
    REQUIRE(below.has_value());
    CHECK(below->x() == doctest::Approx(5.0f).epsilon(0.05));
    CHECK(below->y() == doctest::Approx(2.0f).epsilon(0.05));
    CHECK(std::abs(below->z()) < 0.3f);

    CHECK_FALSE(mesh.SamplePosition(HM::Vector3(5.0f, 2.0f, 5.0f), 2.0f).has_value());   // too far above
    CHECK_FALSE(mesh.SamplePosition(HM::Vector3(100.0f, 0.0f, 0.0f), 2.0f).has_value()); // nowhere near
}

TEST_CASE("Raycast stops at the wall and passes where nothing is in the way")
{
    HN::NavMesh mesh = MakeWalledNavMesh();
    const HN::NavRayHit hit = mesh.Raycast(HM::Vector3(-10.0f, -5.0f, 0.0f), HM::Vector3(10.0f, -5.0f, 0.0f)).value();
    CHECK(hit.Point.x() < -0.3f);
    CHECK(hit.Point.x() > -1.5f); // the wall's face less the agent's radius
    CHECK(hit.Fraction > 0.4f);
    CHECK(hit.Fraction < 0.5f);
    CHECK(hit.Normal.x() < -0.9f); // facing back along the ray

    CHECK_FALSE(mesh.Raycast(HM::Vector3(-10.0f, -5.0f, 0.0f), HM::Vector3(-5.0f, -5.0f, 0.0f)).has_value());
    // Through the gap the ray reaches the far side.
    CHECK_FALSE(mesh.Raycast(HM::Vector3(-10.0f, 4.0f, 0.0f), HM::Vector3(10.0f, 4.0f, 0.0f)).has_value());
}

TEST_CASE("Debug edges outline the polygons on the ground")
{
    HN::NavMesh                                       mesh = MakeWalledNavMesh();
    std::vector<std::pair<HM::Vector3, HM::Vector3>> edges;
    mesh.GetDebugEdges(edges);
    REQUIRE_FALSE(edges.empty());
    for (const auto& [a, b] : edges)
    {
        CHECK(std::abs(a.z()) < 0.3f);
        CHECK(std::abs(b.z()) < 0.3f);
        CHECK((b - a).LengthSlow() > 0.0f);
    }
}

TEST_CASE("Data that is not a navmesh is refused with one error")
{
    HN::NavMesh mesh;
    {
        LogCapture                 log;
        const std::vector<uint8_t> garbage(512, 0xAB);
        CHECK_FALSE(mesh.Load(garbage));
        CHECK_FALSE(mesh.IsLoaded());
        CHECK(log.Lines("[ERROR]").size() == 1);
        CHECK(log.Text().find("is not a navmesh") != std::string::npos);
    }
    {
        LogCapture                 log;
        const std::vector<uint8_t> tiny(8, 0);
        CHECK_FALSE(mesh.Load(tiny));
        CHECK(log.Lines("[ERROR]").size() == 1);
    }

    // A failed load leaves nothing loaded even after a good one.
    mesh = MakeWalledNavMesh();
    REQUIRE(mesh.IsLoaded());
    LogCapture log;
    CHECK_FALSE(mesh.Load(std::vector<uint8_t>(512, 0xAB)));
    CHECK_FALSE(mesh.IsLoaded());
}

TEST_CASE("Queries after the first allocate nothing")
{
    HN::NavMesh                                       mesh = MakeWalledNavMesh();
    std::vector<HM::Vector3>                          corners;
    std::vector<std::pair<HM::Vector3, HM::Vector3>> edges;
    const HM::Vector3 start(-10.0f, -5.0f, 0.0f);
    const HM::Vector3 end(10.0f, -5.0f, 0.0f);
    REQUIRE(mesh.FindPath(start, end, corners) == HN::NavPathStatus::Complete);
    mesh.GetDebugEdges(edges);
    const size_t cornerCapacity = corners.capacity();
    const size_t edgeCapacity   = edges.capacity();

    // The library's own allocations go through its DLL's operator new and Detour's dtAlloc, which
    // this counter does not see; the output vectors keeping their capacity covers those.
    s_Allocations = 0;
    s_Counting    = true;
    for (int i = 0; i < 100; ++i)
    {
        (void)mesh.FindPath(start, end, corners);
        (void)mesh.SamplePosition(HM::Vector3(5.0f, 2.0f, 1.0f), 2.0f);
        (void)mesh.Raycast(start, end);
        mesh.GetDebugEdges(edges);
    }
    s_Counting = false;
    CHECK(s_Allocations == 0);
    CHECK(corners.capacity() == cornerCapacity);
    CHECK(edges.capacity() == edgeCapacity);
}
