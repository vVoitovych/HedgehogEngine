#include "HedgehogNavigation/api/NavigationTypes.hpp"

#include "DetourNavMesh.h"
#include "Recast.h"

namespace HN
{
    std::string CheckNavigationLibraries()
    {
        rcContext       context(false);
        rcHeightfield*  heightfield = rcAllocHeightfield();
        const float     bmin[3]     = { 0.0f, 0.0f, 0.0f };
        const float     bmax[3]     = { 1.0f, 1.0f, 1.0f };
        const bool      created     = heightfield && rcCreateHeightfield(&context, *heightfield, 4, 4, bmin, bmax, 0.25f, 0.25f);
        rcFreeHeightField(heightfield);
        if (!created)
            return "Recast could not create a heightfield.";

        dtNavMesh* navMesh = dtAllocNavMesh();
        if (!navMesh)
            return "Detour could not allocate a navmesh.";
        dtFreeNavMesh(navMesh);
        return {};
    }
}
