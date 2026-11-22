#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "HedgehogNavigation/api/NavigationTypes.hpp"

#include "ECS/api/ECS.hpp"

namespace HedgehogEngine
{
    class ResourceCatalog;

    struct NavigationGeometryOptions
    {
        // Also every rendered mesh that does not move (no dynamic or kinematic rigid body).
        bool IncludeRenderMeshes = false;
    };

    // The world-space triangles (Z-up) a scene's navmesh is baked from:
    //  - every static collider (no rigid body, or a static one; not a trigger) shaped as its body is:
    //    a box's 12 triangles, a sphere and a capsule tessellated, posed by ComputeWorldTransform with
    //    the shape's size times the world scale (a sphere's and capsule's by its largest axis);
    //  - every enabled terrain whose file has loaded (its TerrainIndex resolved), BuildTerrainMesh
    //    placed by its world transform;
    //  - with IncludeRenderMeshes, every resolved MeshComponent mesh through its ObjMatrix.
    // An entity whose RenderComponent (or terrain) is on the editor layer gives nothing.
    [[nodiscard]] HEDGEHOG_ENGINE_API HN::NavMeshGeometry GatherNavigationGeometry(ECS::ECS& ecs, const ResourceCatalog& catalog,
                                                                                  const NavigationGeometryOptions& options = {});
}
