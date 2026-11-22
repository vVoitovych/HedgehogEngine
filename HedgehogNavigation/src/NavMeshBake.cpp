#include "HedgehogNavigation/api/NavMeshBake.hpp"

#include "RecastAxes.hpp"

#include "Logger/api/Logger.hpp"

#include "DetourNavMesh.h"
#include "DetourNavMeshBuilder.h"
#include "Recast.h"

#include <cmath>
#include <memory>
#include <queue>

namespace HN
{
    namespace
    {
        // Every polygon the bake makes is walkable; queries filter on this flag.
        constexpr unsigned short WALKABLE_POLY_FLAG = 1;

        // Recast's messages, through the Logger.
        class LoggingContext : public rcContext
        {
        public:
            LoggingContext() : rcContext(true) {}

        protected:
            void doLog(const rcLogCategory category, const char* message, const int length) override
            {
                const std::string text(message, static_cast<size_t>(length));
                if (category == RC_LOG_ERROR)
                    LOGERROR("[Navigation] ", text);
                else if (category == RC_LOG_WARNING)
                    LOGWARNING("[Navigation] ", text);
            }
        };

        struct HeightfieldDeleter { void operator()(rcHeightfield* p) const { rcFreeHeightField(p); } };
        struct CompactDeleter     { void operator()(rcCompactHeightfield* p) const { rcFreeCompactHeightfield(p); } };
        struct ContourDeleter     { void operator()(rcContourSet* p) const { rcFreeContourSet(p); } };
        struct PolyMeshDeleter    { void operator()(rcPolyMesh* p) const { rcFreePolyMesh(p); } };
        struct DetailDeleter      { void operator()(rcPolyMeshDetail* p) const { rcFreePolyMeshDetail(p); } };
        struct NavMeshDeleter     { void operator()(dtNavMesh* p) const { dtFreeNavMesh(p); } };

        bool IsFinite(const HM::Vector3& v)
        {
            return std::isfinite(v.x()) && std::isfinite(v.y()) && std::isfinite(v.z());
        }

        // Why the input cannot be baked, or empty.
        std::string CheckInput(const NavMeshGeometry& geometry, const NavMeshBuildSettings& s)
        {
            if (geometry.Indices.empty())
                return "the geometry has no triangles";
            if (geometry.Indices.size() % 3 != 0)
                return "the geometry has " + std::to_string(geometry.Indices.size()) + " indices, not a whole number of triangles";
            for (size_t i = 0; i < geometry.Indices.size(); ++i)
                if (geometry.Indices[i] >= geometry.Positions.size())
                    return "index " + std::to_string(i) + " (" + std::to_string(geometry.Indices[i]) + ") is past the " +
                           std::to_string(geometry.Positions.size()) + " positions";
            for (size_t i = 0; i < geometry.Positions.size(); ++i)
                if (!IsFinite(geometry.Positions[i]))
                    return "position " + std::to_string(i) + " is not finite";
            const bool positive = s.CellSize > 0.0f && s.CellHeight > 0.0f && s.AgentHeight > 0.0f && s.AgentMaxSlope > 0.0f &&
                                  s.AgentMaxSlope < 90.0f && s.RegionMinSize >= 0.0f && s.RegionMergeSize >= 0.0f &&
                                  s.AgentRadius >= 0.0f && s.AgentMaxClimb >= 0.0f && s.EdgeMaxLength >= 0.0f &&
                                  s.EdgeMaxError > 0.0f && s.DetailSampleDistance >= 0.0f && s.DetailSampleMaxError >= 0.0f &&
                                  s.VertsPerPoly >= 3 && s.VertsPerPoly <= DT_VERTS_PER_POLYGON;
            if (!positive)
                return "the build settings are out of range";
            return {};
        }

        NavMeshBakeResult Fail(const std::string& error)
        {
            NavMeshBakeResult result;
            result.Error = error;
            return result;
        }
    }

    NavMeshBakeResult BakeNavMesh(const NavMeshGeometry& geometry, const NavMeshBuildSettings& settings)
    {
        if (const std::string error = CheckInput(geometry, settings); !error.empty())
            return Fail("A navmesh cannot be baked: " + error + ".");

        // The input in Recast's axes and winding.
        std::vector<float> verts;
        verts.reserve(geometry.Positions.size() * 3);
        for (const HM::Vector3& position : geometry.Positions)
        {
            const auto p = ToRecast(position);
            verts.insert(verts.end(), p.begin(), p.end());
        }
        std::vector<int> tris;
        tris.reserve(geometry.Indices.size());
        for (size_t i = 0; i < geometry.Indices.size(); i += 3)
        {
            const auto t = ToRecastTriangle(geometry.Indices[i], geometry.Indices[i + 1], geometry.Indices[i + 2]);
            tris.insert(tris.end(), t.begin(), t.end());
        }
        const int vertCount = static_cast<int>(geometry.Positions.size());
        const int triCount  = static_cast<int>(geometry.Indices.size() / 3);

        // rcConfig from the settings, in cells as Recast's samples derive it.
        const float cs = settings.CellSize;
        const float ch = settings.CellHeight;
        const int   walkableHeight = static_cast<int>(std::ceil(settings.AgentHeight / ch));
        const int   walkableClimb  = static_cast<int>(std::floor(settings.AgentMaxClimb / ch));
        const int   walkableRadius = static_cast<int>(std::ceil(settings.AgentRadius / cs));
        const int   maxEdgeLength  = static_cast<int>(settings.EdgeMaxLength / cs);
        const int   minRegionArea  = static_cast<int>(settings.RegionMinSize * settings.RegionMinSize);
        const int   mergeRegionArea = static_cast<int>(settings.RegionMergeSize * settings.RegionMergeSize);
        const float detailSampleDistance = settings.DetailSampleDistance < 0.9f ? 0.0f : cs * settings.DetailSampleDistance;
        const float detailSampleError    = ch * settings.DetailSampleMaxError;

        float bmin[3];
        float bmax[3];
        rcCalcBounds(verts.data(), vertCount, bmin, bmax);
        int width  = 0;
        int height = 0;
        rcCalcGridSize(bmin, bmax, cs, &width, &height);

        LoggingContext context;

        // 1. The heightfield.
        std::unique_ptr<rcHeightfield, HeightfieldDeleter> solid(rcAllocHeightfield());
        if (!solid || !rcCreateHeightfield(&context, *solid, width, height, bmin, bmax, cs, ch))
            return Fail("A navmesh cannot be baked: the heightfield could not be made.");

        // 2. Walkable triangles, rasterized and filtered.
        std::vector<unsigned char> areas(static_cast<size_t>(triCount), 0);
        rcMarkWalkableTriangles(&context, settings.AgentMaxSlope, verts.data(), vertCount, tris.data(), triCount, areas.data());
        if (!rcRasterizeTriangles(&context, verts.data(), vertCount, tris.data(), areas.data(), triCount, *solid, walkableClimb))
            return Fail("A navmesh cannot be baked: the triangles could not be rasterized.");
        rcFilterLowHangingWalkableObstacles(&context, walkableClimb, *solid);
        rcFilterLedgeSpans(&context, walkableHeight, walkableClimb, *solid);
        rcFilterWalkableLowHeightSpans(&context, walkableHeight, *solid);

        // 3. Compact heightfield, erosion, regions.
        std::unique_ptr<rcCompactHeightfield, CompactDeleter> compact(rcAllocCompactHeightfield());
        if (!compact || !rcBuildCompactHeightfield(&context, walkableHeight, walkableClimb, *solid, *compact))
            return Fail("A navmesh cannot be baked: the compact heightfield could not be built.");
        solid.reset();
        if (!rcErodeWalkableArea(&context, walkableRadius, *compact))
            return Fail("A navmesh cannot be baked: the walkable area could not be eroded.");
        if (!rcBuildDistanceField(&context, *compact))
            return Fail("A navmesh cannot be baked: the distance field could not be built.");
        if (!rcBuildRegions(&context, *compact, 0, minRegionArea, mergeRegionArea))
            return Fail("A navmesh cannot be baked: the regions could not be built.");

        // 4. Contours, polygons, detail.
        std::unique_ptr<rcContourSet, ContourDeleter> contours(rcAllocContourSet());
        if (!contours || !rcBuildContours(&context, *compact, settings.EdgeMaxError, maxEdgeLength, *contours))
            return Fail("A navmesh cannot be baked: the contours could not be built.");
        std::unique_ptr<rcPolyMesh, PolyMeshDeleter> polyMesh(rcAllocPolyMesh());
        if (!polyMesh || !rcBuildPolyMesh(&context, *contours, settings.VertsPerPoly, *polyMesh))
            return Fail("A navmesh cannot be baked: the polygon mesh could not be built.");
        std::unique_ptr<rcPolyMeshDetail, DetailDeleter> detail(rcAllocPolyMeshDetail());
        if (!detail || !rcBuildPolyMeshDetail(&context, *polyMesh, *compact, detailSampleDistance, detailSampleError, *detail))
            return Fail("A navmesh cannot be baked: the detail mesh could not be built.");
        if (polyMesh->npolys == 0)
            return Fail("A navmesh cannot be baked: no area is walkable.");
        if (polyMesh->nverts >= 0xffff)
            return Fail("A navmesh cannot be baked: it has more vertices than one tile holds.");

        // 5. Detour's data.
        for (int i = 0; i < polyMesh->npolys; ++i)
            polyMesh->flags[i] = polyMesh->areas[i] == RC_WALKABLE_AREA ? WALKABLE_POLY_FLAG : 0;
        dtNavMeshCreateParams params{};
        params.verts            = polyMesh->verts;
        params.vertCount        = polyMesh->nverts;
        params.polys            = polyMesh->polys;
        params.polyAreas        = polyMesh->areas;
        params.polyFlags        = polyMesh->flags;
        params.polyCount        = polyMesh->npolys;
        params.nvp              = polyMesh->nvp;
        params.detailMeshes     = detail->meshes;
        params.detailVerts      = detail->verts;
        params.detailVertsCount = detail->nverts;
        params.detailTris       = detail->tris;
        params.detailTriCount   = detail->ntris;
        params.walkableHeight   = settings.AgentHeight;
        params.walkableRadius   = settings.AgentRadius;
        params.walkableClimb    = settings.AgentMaxClimb;
        rcVcopy(params.bmin, polyMesh->bmin);
        rcVcopy(params.bmax, polyMesh->bmax);
        params.cs          = cs;
        params.ch          = ch;
        params.buildBvTree = true;

        unsigned char* navData     = nullptr;
        int            navDataSize = 0;
        if (!dtCreateNavMeshData(&params, &navData, &navDataSize))
            return Fail("A navmesh cannot be baked: Detour could not make its data.");
        NavMeshBakeResult result;
        result.Data.assign(navData, navData + navDataSize);
        result.PolygonCount = static_cast<uint32_t>(polyMesh->npolys);
        dtFree(navData);
        return result;
    }

    std::optional<NavMeshDescription> DescribeNavMesh(const std::vector<uint8_t>& data)
    {
        if (data.empty())
            return std::nullopt;
        // Detour fixes its pointers up inside the data, so it reads a copy it does not own.
        std::vector<uint8_t>                     copy = data;
        std::unique_ptr<dtNavMesh, NavMeshDeleter> navMesh(dtAllocNavMesh());
        if (!navMesh || dtStatusFailed(navMesh->init(copy.data(), static_cast<int>(copy.size()), 0)))
            return std::nullopt;
        const dtNavMesh&  mesh = *navMesh; // the public getTile is the const one
        const dtMeshTile* tile = mesh.getTile(0);
        if (!tile || !tile->header)
            return std::nullopt;

        NavMeshDescription description;
        const int          polyCount = tile->header->polyCount;
        description.PolygonCentres.resize(static_cast<size_t>(polyCount));
        description.Islands.assign(static_cast<size_t>(polyCount), UINT32_MAX);
        for (int i = 0; i < polyCount; ++i)
        {
            const dtPoly& poly = tile->polys[i];
            float         centre[3] = { 0.0f, 0.0f, 0.0f };
            for (int v = 0; v < poly.vertCount; ++v)
                for (int axis = 0; axis < 3; ++axis)
                    centre[axis] += tile->verts[poly.verts[v] * 3 + axis] / static_cast<float>(poly.vertCount);
            description.PolygonCentres[static_cast<size_t>(i)] = FromRecast(centre);
        }

        // Islands by walking each polygon's links to its neighbours.
        const dtPolyRef base = mesh.getPolyRefBase(tile);
        for (int start = 0; start < polyCount; ++start)
        {
            if (description.Islands[static_cast<size_t>(start)] != UINT32_MAX)
                continue;
            const uint32_t island = description.IslandCount++;
            std::queue<int> open;
            open.push(start);
            description.Islands[static_cast<size_t>(start)] = island;
            while (!open.empty())
            {
                const int current = open.front();
                open.pop();
                for (unsigned int link = tile->polys[current].firstLink; link != DT_NULL_LINK; link = tile->links[link].next)
                {
                    const int neighbour = static_cast<int>(tile->links[link].ref - base);
                    if (neighbour >= 0 && neighbour < polyCount && description.Islands[static_cast<size_t>(neighbour)] == UINT32_MAX)
                    {
                        description.Islands[static_cast<size_t>(neighbour)] = island;
                        open.push(neighbour);
                    }
                }
            }
        }
        return description;
    }
}
