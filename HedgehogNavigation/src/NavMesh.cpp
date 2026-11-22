#include "HedgehogNavigation/api/NavMesh.hpp"

#include "RecastAxes.hpp"

#include "Logger/api/Logger.hpp"

#include "DetourAlloc.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace HN
{
    struct NavMeshDeleter      { void operator()(dtNavMesh* p) const { dtFreeNavMesh(p); } };
    struct NavMeshQueryDeleter { void operator()(dtNavMeshQuery* p) const { dtFreeNavMeshQuery(p); } };

    struct NavMeshState
    {
        std::unique_ptr<dtNavMesh, NavMeshDeleter>           Mesh;
        std::unique_ptr<dtNavMeshQuery, NavMeshQueryDeleter> Query;
        dtQueryFilter                                        Filter;
        float                                                SearchExtents[3] = { 1.0f, 1.0f, 1.0f }; // Recast axes

        // Scratch, sized once, so queries allocate nothing.
        std::array<dtPolyRef, MAX_PATH_POLYGONS> Polygons{};
        std::array<float, MAX_PATH_CORNERS * 3>  Corners{};
    };

    namespace
    {
        const dtMeshTile* GetTile(const dtNavMesh& mesh)
        {
            const dtMeshTile* tile = mesh.getTile(0);
            return tile && tile->header ? tile : nullptr;
        }

        // The nearest polygon to point within the extents and the point on it; 0 for none.
        dtPolyRef Snap(const NavMeshState& state, const HM::Vector3& point, const float* extents, float* snapped)
        {
            const auto      p   = ToRecast(point);
            dtPolyRef       ref = 0;
            const dtStatus  status = state.Query->findNearestPoly(p.data(), extents, &state.Filter, &ref, snapped);
            return dtStatusSucceed(status) ? ref : 0;
        }
    }

    NavMesh::NavMesh() = default;
    NavMesh::~NavMesh() = default;
    NavMesh::NavMesh(NavMesh&&) noexcept = default;
    NavMesh& NavMesh::operator=(NavMesh&&) noexcept = default;

    bool NavMesh::Load(std::span<const uint8_t> data)
    {
        Unload();
        if (data.size() < sizeof(dtMeshHeader))
        {
            LOGERROR("[Navigation] The data (", data.size(), " bytes) is too short to be a navmesh.");
            return false;
        }

        auto state = std::make_unique<NavMeshState>();
        state->Mesh.reset(dtAllocNavMesh());
        state->Query.reset(dtAllocNavMeshQuery());
        // Detour owns its copy (DT_TILE_FREE_DATA) and fixes its pointers up inside it.
        auto* copy = static_cast<unsigned char*>(dtAlloc(data.size(), DT_ALLOC_PERM));
        if (!state->Mesh || !state->Query || !copy)
        {
            dtFree(copy);
            LOGERROR("[Navigation] A navmesh could not be allocated.");
            return false;
        }
        std::memcpy(copy, data.data(), data.size());
        if (dtStatusFailed(state->Mesh->init(copy, static_cast<int>(data.size()), DT_TILE_FREE_DATA)))
        {
            dtFree(copy);
            LOGERROR("[Navigation] The data (", data.size(), " bytes) is not a navmesh.");
            return false;
        }
        if (dtStatusFailed(state->Query->init(state->Mesh.get(), MAX_SEARCH_NODES)))
        {
            LOGERROR("[Navigation] The navmesh's query could not be made.");
            return false;
        }

        const dtMeshTile* tile = GetTile(*state->Mesh);
        if (!tile)
        {
            LOGERROR("[Navigation] The navmesh has no tile.");
            return false;
        }
        const float across        = std::max(2.0f * tile->header->walkableRadius, MIN_SEARCH_HALF_EXTENT);
        state->SearchExtents[0]   = across;
        state->SearchExtents[1]   = std::max(tile->header->walkableHeight, MIN_SEARCH_HALF_EXTENT);
        state->SearchExtents[2]   = across;
        m_State                   = std::move(state);
        return true;
    }

    void NavMesh::Unload()
    {
        m_State.reset();
    }

    bool NavMesh::IsLoaded() const
    {
        return m_State != nullptr;
    }

    NavPathStatus NavMesh::FindPath(const HM::Vector3& start, const HM::Vector3& end, std::vector<HM::Vector3>& outCorners)
    {
        outCorners.clear();
        if (!m_State)
            return NavPathStatus::None;
        NavMeshState& s = *m_State;

        float           startPoint[3];
        float           endPoint[3];
        const dtPolyRef startRef = Snap(s, start, s.SearchExtents, startPoint);
        const dtPolyRef endRef   = Snap(s, end, s.SearchExtents, endPoint);
        if (startRef == 0 || endRef == 0)
            return NavPathStatus::None;

        int            polygonCount = 0;
        const dtStatus pathStatus   = s.Query->findPath(startRef, endRef, startPoint, endPoint, &s.Filter, s.Polygons.data(),
                                                        &polygonCount, MAX_PATH_POLYGONS);
        if (dtStatusFailed(pathStatus) || polygonCount == 0)
            return NavPathStatus::None;

        // A path that stops short ends at the point of its last polygon nearest the end.
        const bool reached = s.Polygons[static_cast<size_t>(polygonCount - 1)] == endRef &&
                             !dtStatusDetail(pathStatus, DT_PARTIAL_RESULT);
        float target[3] = { endPoint[0], endPoint[1], endPoint[2] };
        if (!reached)
            s.Query->closestPointOnPoly(s.Polygons[static_cast<size_t>(polygonCount - 1)], endPoint, target, nullptr);

        int cornerCount = 0;
        if (dtStatusFailed(s.Query->findStraightPath(startPoint, target, s.Polygons.data(), polygonCount, s.Corners.data(),
                                                     nullptr, nullptr, &cornerCount, MAX_PATH_CORNERS)) ||
            cornerCount == 0)
            return NavPathStatus::None;

        for (int i = 0; i < cornerCount; ++i)
            outCorners.push_back(FromRecast(&s.Corners[static_cast<size_t>(i) * 3]));
        return reached ? NavPathStatus::Complete : NavPathStatus::Partial;
    }

    std::optional<HM::Vector3> NavMesh::SamplePosition(const HM::Vector3& point, float maxDistance)
    {
        if (!m_State || !(maxDistance > 0.0f))
            return std::nullopt;
        const float extents[3] = { maxDistance, maxDistance, maxDistance };
        float       snapped[3];
        if (Snap(*m_State, point, extents, snapped) == 0)
            return std::nullopt;
        return FromRecast(snapped);
    }

    std::optional<NavRayHit> NavMesh::Raycast(const HM::Vector3& start, const HM::Vector3& end)
    {
        if (!m_State)
            return std::nullopt;
        NavMeshState& s = *m_State;

        float           startPoint[3];
        const dtPolyRef startRef = Snap(s, start, s.SearchExtents, startPoint);
        if (startRef == 0)
            return std::nullopt;

        const auto endPoint    = ToRecast(end);
        float      t           = 0.0f;
        float      normal[3]   = { 0.0f, 0.0f, 0.0f };
        int        visited     = 0;
        if (dtStatusFailed(s.Query->raycast(startRef, startPoint, endPoint.data(), &s.Filter, &t, normal, s.Polygons.data(),
                                            &visited, MAX_PATH_POLYGONS)) ||
            t > 1.0f)
            return std::nullopt;

        float hit[3];
        for (int axis = 0; axis < 3; ++axis)
            hit[axis] = startPoint[axis] + (endPoint[static_cast<size_t>(axis)] - startPoint[axis]) * t;
        return NavRayHit{ FromRecast(hit), FromRecast(normal), t };
    }

    void NavMesh::GetDebugEdges(std::vector<std::pair<HM::Vector3, HM::Vector3>>& out) const
    {
        out.clear();
        if (!m_State)
            return;
        const dtMeshTile* tile = GetTile(*m_State->Mesh);
        if (!tile)
            return;
        for (int i = 0; i < tile->header->polyCount; ++i)
        {
            const dtPoly& poly = tile->polys[i];
            if (poly.getType() == DT_POLYTYPE_OFFMESH_CONNECTION)
                continue;
            for (int v = 0; v < poly.vertCount; ++v)
            {
                // An edge two polygons of the tile share is drawn by the lower-numbered one.
                const unsigned short neighbour = poly.neis[v];
                if (neighbour != 0 && (neighbour & DT_EXT_LINK) == 0 && neighbour - 1 < i)
                    continue;
                const float* a = &tile->verts[poly.verts[v] * 3];
                const float* b = &tile->verts[poly.verts[(v + 1) % poly.vertCount] * 3];
                out.emplace_back(FromRecast(a), FromRecast(b));
            }
        }
    }
}
