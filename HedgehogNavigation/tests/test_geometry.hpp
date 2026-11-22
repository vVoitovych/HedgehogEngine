#pragma once

// Geometry the navigation tests bake. Test code only.

#include "HedgehogNavigation/api/NavigationTypes.hpp"

namespace NavTest
{
    using HN::NavMeshGeometry;

    // A quad of four corners, counterclockwise seen from its walkable side.
    inline void AddQuad(NavMeshGeometry& g, const HM::Vector3& a, const HM::Vector3& b, const HM::Vector3& c, const HM::Vector3& d)
    {
        const auto first = static_cast<uint32_t>(g.Positions.size());
        g.Positions.insert(g.Positions.end(), { a, b, c, d });
        g.Indices.insert(g.Indices.end(), { first, first + 1, first + 2, first, first + 2, first + 3 });
    }

    // A horizontal rectangle at height z, facing up.
    inline void AddFloor(NavMeshGeometry& g, float x0, float y0, float x1, float y1, float z)
    {
        AddQuad(g, HM::Vector3(x0, y0, z), HM::Vector3(x1, y0, z), HM::Vector3(x1, y1, z), HM::Vector3(x0, y1, z));
    }

    // A closed box from (x0, y0, 0) to (x1, y1, h): its top and four sides.
    inline void AddBox(NavMeshGeometry& g, float x0, float y0, float x1, float y1, float h)
    {
        AddFloor(g, x0, y0, x1, y1, h);
        AddQuad(g, HM::Vector3(x0, y0, 0.0f), HM::Vector3(x0, y0, h), HM::Vector3(x1, y0, h), HM::Vector3(x1, y0, 0.0f));
        AddQuad(g, HM::Vector3(x1, y0, 0.0f), HM::Vector3(x1, y0, h), HM::Vector3(x1, y1, h), HM::Vector3(x1, y1, 0.0f));
        AddQuad(g, HM::Vector3(x1, y1, 0.0f), HM::Vector3(x1, y1, h), HM::Vector3(x0, y1, h), HM::Vector3(x0, y1, 0.0f));
        AddQuad(g, HM::Vector3(x0, y1, 0.0f), HM::Vector3(x0, y1, h), HM::Vector3(x0, y0, h), HM::Vector3(x0, y0, 0.0f));
    }

}
