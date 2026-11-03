#include "HedgehogEngine/api/Terrain/TerrainGeometry.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace HedgehogEngine
{
    namespace
    {
        constexpr float RAY_EPSILON = 1.0e-6f;

        float Height(const TerrainData& data, uint32_t column, uint32_t row)
        {
            const size_t index = static_cast<size_t>(row) * data.Resolution + column;
            return data.Heights.size() == static_cast<size_t>(data.Resolution) * data.Resolution ? data.Heights[index] : 0.0f;
        }

        HM::Vector3 Corner(const TerrainData& data, uint32_t column, uint32_t row)
        {
            const float spacing = GetTerrainSpacing(data);
            return HM::Vector3(column * spacing, row * spacing, Height(data, column, row));
        }

        // The ray's distance to triangle (a, b, c), from either side (Moller-Trumbore).
        std::optional<float> IntersectTriangle(const HM::Vector3& origin, const HM::Vector3& direction,
                                               const HM::Vector3& a, const HM::Vector3& b, const HM::Vector3& c)
        {
            const HM::Vector3 edge1       = b - a;
            const HM::Vector3 edge2       = c - a;
            const HM::Vector3 p           = HM::Cross(direction, edge2);
            const float       determinant = HM::Dot(edge1, p);
            if (std::abs(determinant) < RAY_EPSILON)
                return std::nullopt;
            const float       inverse = 1.0f / determinant;
            const HM::Vector3 s       = origin - a;
            const float       u       = HM::Dot(s, p) * inverse;
            if (u < -RAY_EPSILON || u > 1.0f + RAY_EPSILON)
                return std::nullopt;
            const HM::Vector3 q = HM::Cross(s, edge1);
            const float       v = HM::Dot(direction, q) * inverse;
            if (v < -RAY_EPSILON || u + v > 1.0f + RAY_EPSILON)
                return std::nullopt;
            return HM::Dot(edge2, q) * inverse;
        }

        // Where the ray enters and leaves the box; nothing when it misses it.
        std::optional<std::pair<float, float>> ClipToBox(const HM::Vector3& origin, const HM::Vector3& direction,
                                                         const HM::Vector3& low, const HM::Vector3& high)
        {
            float enter = 0.0f;
            float exit  = std::numeric_limits<float>::max();
            for (size_t axis = 0; axis < 3; ++axis)
            {
                if (std::abs(direction[axis]) < RAY_EPSILON)
                {
                    if (origin[axis] < low[axis] || origin[axis] > high[axis])
                        return std::nullopt;
                    continue;
                }
                float near = (low[axis] - origin[axis]) / direction[axis];
                float far  = (high[axis] - origin[axis]) / direction[axis];
                if (near > far)
                    std::swap(near, far);
                enter = std::max(enter, near);
                exit  = std::min(exit, far);
                if (enter > exit)
                    return std::nullopt;
            }
            return std::make_pair(enter, exit);
        }
    }

    float GetTerrainSpacing(const TerrainData& data)
    {
        return data.Resolution > 1 ? data.Size / static_cast<float>(data.Resolution - 1) : data.Size;
    }

    TerrainMesh BuildTerrainMesh(const TerrainData& data)
    {
        TerrainMesh mesh;
        if (data.Resolution < 2)
            return mesh;

        const uint32_t resolution = data.Resolution;
        const float    spacing    = GetTerrainSpacing(data);
        const size_t   count      = static_cast<size_t>(resolution) * resolution;
        mesh.Positions.reserve(count);
        mesh.TexCoords.reserve(count);
        mesh.Normals.reserve(count);
        mesh.Tangents.reserve(count);
        for (uint32_t row = 0; row < resolution; ++row)
        {
            for (uint32_t column = 0; column < resolution; ++column)
            {
                mesh.Positions.push_back(Corner(data, column, row));
                mesh.TexCoords.push_back(HM::Vector2(static_cast<float>(column) / (resolution - 1),
                                                     static_cast<float>(row) / (resolution - 1)));

                // Central differences, one-sided at the edges.
                const uint32_t left  = column > 0 ? column - 1 : column;
                const uint32_t right = column + 1 < resolution ? column + 1 : column;
                const uint32_t down  = row > 0 ? row - 1 : row;
                const uint32_t up    = row + 1 < resolution ? row + 1 : row;
                const float    dx    = (Height(data, right, row) - Height(data, left, row)) / ((right - left) * spacing);
                const float    dy    = (Height(data, column, up) - Height(data, column, down)) / ((up - down) * spacing);
                const HM::Vector3 normal = HM::Vector3(-dx, -dy, 1.0f).Normalize();
                mesh.Normals.push_back(normal);

                const HM::Vector3 alongX  = HM::Vector3(1.0f, 0.0f, dx);
                const HM::Vector3 tangent = (alongX - normal * HM::Dot(normal, alongX)).Normalize();
                mesh.Tangents.push_back(HM::Vector4(tangent.x(), tangent.y(), tangent.z(), 1.0f));
            }
        }

        mesh.Indices.reserve(static_cast<size_t>(resolution - 1) * (resolution - 1) * 6);
        for (uint32_t row = 0; row + 1 < resolution; ++row)
        {
            for (uint32_t column = 0; column + 1 < resolution; ++column)
            {
                const uint32_t v00 = row * resolution + column;
                const uint32_t v10 = v00 + 1;
                const uint32_t v01 = v00 + resolution;
                const uint32_t v11 = v01 + 1;
                mesh.Indices.insert(mesh.Indices.end(), { v00, v10, v11, v00, v11, v01 });
            }
        }
        return mesh;
    }

    float SampleTerrainHeight(const TerrainData& data, float x, float y)
    {
        if (data.Resolution < 2 || !std::isfinite(x) || !std::isfinite(y))
            return 0.0f;
        const float    spacing = GetTerrainSpacing(data);
        const float    fx      = std::clamp(x, 0.0f, data.Size) / spacing;
        const float    fy      = std::clamp(y, 0.0f, data.Size) / spacing;
        const uint32_t column  = std::min(static_cast<uint32_t>(fx), data.Resolution - 2);
        const uint32_t row     = std::min(static_cast<uint32_t>(fy), data.Resolution - 2);
        const float    u       = fx - column;
        const float    v       = fy - row;

        const float h00 = Height(data, column, row);
        const float h10 = Height(data, column + 1, row);
        const float h01 = Height(data, column, row + 1);
        const float h11 = Height(data, column + 1, row + 1);
        return u >= v ? h00 + u * (h10 - h00) + v * (h11 - h10) : h00 + v * (h01 - h00) + u * (h11 - h01);
    }

    std::optional<TerrainRayHit> RaycastTerrain(const TerrainData& data, const HM::Vector3& origin,
                                                const HM::Vector3& direction, float maxDistance)
    {
        const float length = direction.LengthSlow();
        if (data.Resolution < 2 || !(length > RAY_EPSILON) || !(maxDistance > 0.0f) || !std::isfinite(length))
            return std::nullopt;
        const HM::Vector3 ray = direction / length;

        float lowest  = 0.0f;
        float highest = 0.0f;
        if (data.Heights.size() == static_cast<size_t>(data.Resolution) * data.Resolution)
        {
            const auto [low, high] = std::minmax_element(data.Heights.begin(), data.Heights.end());
            lowest                 = *low;
            highest                = *high;
        }
        const float margin  = 1.0e-3f;
        const auto  clipped = ClipToBox(origin, ray, HM::Vector3(-margin, -margin, lowest - margin),
                                        HM::Vector3(data.Size + margin, data.Size + margin, highest + margin));
        if (!clipped || clipped->first > maxDistance)
            return std::nullopt;
        const float exit = std::min(clipped->second, maxDistance);

        // Walk the cells the ray crosses in order (2D DDA); the first hit is the nearest.
        const float       spacing = GetTerrainSpacing(data);
        const uint32_t    last    = data.Resolution - 2;
        const HM::Vector3 entry   = origin + ray * clipped->first;
        const auto        cellOf  = [&](float coordinate)
        { return static_cast<int>(std::clamp(std::floor(coordinate / spacing), 0.0f, static_cast<float>(last))); };
        int column = cellOf(entry.x());
        int row    = cellOf(entry.y());

        const int   stepX   = ray.x() > 0.0f ? 1 : -1;
        const int   stepY   = ray.y() > 0.0f ? 1 : -1;
        const float inf     = std::numeric_limits<float>::infinity();
        const auto  nextHit = [&](float originCoordinate, float rayCoordinate, int cell, int step)
        {
            if (std::abs(rayCoordinate) < RAY_EPSILON)
                return inf;
            const float boundary = (cell + (step > 0 ? 1 : 0)) * spacing;
            return (boundary - originCoordinate) / rayCoordinate;
        };
        float       nextX  = nextHit(origin.x(), ray.x(), column, stepX);
        float       nextY  = nextHit(origin.y(), ray.y(), row, stepY);
        const float deltaX = std::abs(ray.x()) < RAY_EPSILON ? inf : spacing / std::abs(ray.x());
        const float deltaY = std::abs(ray.y()) < RAY_EPSILON ? inf : spacing / std::abs(ray.y());

        while (true)
        {
            const uint32_t    c   = static_cast<uint32_t>(column);
            const uint32_t    r   = static_cast<uint32_t>(row);
            const HM::Vector3 v00 = Corner(data, c, r);
            const HM::Vector3 v10 = Corner(data, c + 1, r);
            const HM::Vector3 v01 = Corner(data, c, r + 1);
            const HM::Vector3 v11 = Corner(data, c + 1, r + 1);

            std::optional<TerrainRayHit> best;
            const auto test = [&](const HM::Vector3& a, const HM::Vector3& b, const HM::Vector3& d)
            {
                const std::optional<float> t = IntersectTriangle(origin, ray, a, b, d);
                if (!t || *t < 0.0f || *t > exit || (best && *t >= best->Distance))
                    return;
                HM::Vector3 normal = HM::Cross(b - a, d - a).Normalize();
                if (normal.z() < 0.0f)
                    normal = normal * -1.0f;
                best = TerrainRayHit{ origin + ray * *t, normal, *t };
            };
            test(v00, v10, v11);
            test(v00, v11, v01);
            if (best)
                return best;

            if (nextX < nextY)
            {
                if (nextX > exit)
                    break;
                column += stepX;
                nextX += deltaX;
            }
            else
            {
                if (nextY > exit)
                    break;
                row += stepY;
                nextY += deltaY;
            }
            if (column < 0 || row < 0 || column > static_cast<int>(last) || row > static_cast<int>(last))
                break;
        }
        return std::nullopt;
    }
}
