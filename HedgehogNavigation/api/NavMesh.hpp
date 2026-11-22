#pragma once

#include "HedgehogNavigationApi.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

// A loaded navmesh and its path queries (Detour), Z-up like the rest of the engine. Detour stays
// behind a forward-declared state, as PhysicsWorld keeps Jolt (module boundary rule 6).
namespace HN
{
    struct NavMeshState;

    enum class NavPathStatus
    {
        None,     // no path: not loaded, or the start or the end is not near the mesh
        Complete, // the path reaches the end
        Partial,  // the end cannot be reached; the path ends at the nearest point that can
    };

    struct NavRayHit
    {
        HM::Vector3 Point;    // where the walk along the surface stops
        HM::Vector3 Normal;   // the wall's normal there, horizontal
        float       Fraction = 0.0f; // of the way from start to end
    };

    // The search box queries snap points within, from a navmesh's agent: twice its radius (at least
    // MIN_SEARCH_HALF_EXTENT) across, its height up and down.
    inline constexpr float MIN_SEARCH_HALF_EXTENT = 1.0f;

    // The polygons a path may cross and the corners it may have; longer paths are cut at the limit.
    inline constexpr int MAX_PATH_POLYGONS = 256;
    inline constexpr int MAX_PATH_CORNERS  = 256;
    // Detour's search nodes, allocated once at Load so a query allocates nothing.
    inline constexpr int MAX_SEARCH_NODES = 2048;

    class NavMesh
    {
    public:
        HEDGEHOG_NAVIGATION_API NavMesh();
        HEDGEHOG_NAVIGATION_API ~NavMesh();
        HEDGEHOG_NAVIGATION_API NavMesh(NavMesh&&) noexcept;
        HEDGEHOG_NAVIGATION_API NavMesh& operator=(NavMesh&&) noexcept;

        NavMesh(const NavMesh&)            = delete;
        NavMesh& operator=(const NavMesh&) = delete;

        // Loads data BakeNavMesh made, replacing what was loaded. False, with one [Navigation] error
        // and nothing loaded, for data Detour does not accept.
        HEDGEHOG_NAVIGATION_API bool Load(std::span<const uint8_t> data);
        HEDGEHOG_NAVIGATION_API void Unload();
        [[nodiscard]] HEDGEHOG_NAVIGATION_API bool IsLoaded() const;

        // The corners of the shortest path from start to end, both snapped onto the mesh within the
        // search box, start and end included, into outCorners (cleared first; its storage reused).
        HEDGEHOG_NAVIGATION_API NavPathStatus FindPath(const HM::Vector3& start, const HM::Vector3& end,
                                                       std::vector<HM::Vector3>& outCorners);

        // The point on the mesh nearest point within maxDistance along every axis, or nullopt.
        [[nodiscard]] HEDGEHOG_NAVIGATION_API std::optional<HM::Vector3> SamplePosition(const HM::Vector3& point,
                                                                                        float maxDistance);

        // Walks a straight line along the surface from start (snapped onto the mesh) towards end:
        // where it meets the mesh's edge, or nullopt when it gets there.
        [[nodiscard]] HEDGEHOG_NAVIGATION_API std::optional<NavRayHit> Raycast(const HM::Vector3& start,
                                                                               const HM::Vector3& end);

        // Every polygon edge once, shared ones included once, for drawing; into out (cleared first).
        HEDGEHOG_NAVIGATION_API void GetDebugEdges(std::vector<std::pair<HM::Vector3, HM::Vector3>>& out) const;

    private:
        std::unique_ptr<NavMeshState> m_State;
    };
}
