#include "api/Tangents.hpp"

#include <cmath>

namespace ContentLoader
{
    namespace
    {
        float Dot(const HM::Vector3& a, const HM::Vector3& b)
        {
            return a.x() * b.x() + a.y() * b.y() + a.z() * b.z();
        }

        HM::Vector3 Cross(const HM::Vector3& a, const HM::Vector3& b)
        {
            return HM::Vector3(a.y() * b.z() - a.z() * b.y(), a.z() * b.x() - a.x() * b.z(),
                               a.x() * b.y() - a.y() * b.x());
        }

        float Length(const HM::Vector3& v)
        {
            return std::sqrt(Dot(v, v));
        }

        // A unit vector orthogonal to the unit normal: the world axis least aligned with it, made
        // orthogonal.
        HM::Vector3 AnyTangent(const HM::Vector3& normal)
        {
            const HM::Vector3 axis = std::abs(normal.x()) < 0.9f ? HM::Vector3(1.0f, 0.0f, 0.0f)
                                                                  : HM::Vector3(0.0f, 1.0f, 0.0f);
            const HM::Vector3 tangent = axis - normal * Dot(normal, axis);
            return tangent / Length(tangent);
        }

        constexpr float EPSILON = 1e-12f;
    }

    void GenerateTangents(std::vector<LoadedVertexData>& vertices, const std::vector<uint32_t>& indices)
    {
        std::vector<HM::Vector3> tangents(vertices.size(), HM::Vector3(0.0f, 0.0f, 0.0f));
        std::vector<HM::Vector3> bitangents(vertices.size(), HM::Vector3(0.0f, 0.0f, 0.0f));

        for (size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            const uint32_t a = indices[i], b = indices[i + 1], c = indices[i + 2];
            if (a >= vertices.size() || b >= vertices.size() || c >= vertices.size())
                continue;

            const HM::Vector3 edge1 = vertices[b].position - vertices[a].position;
            const HM::Vector3 edge2 = vertices[c].position - vertices[a].position;
            const float du1 = vertices[b].uv.x() - vertices[a].uv.x(), dv1 = vertices[b].uv.y() - vertices[a].uv.y();
            const float du2 = vertices[c].uv.x() - vertices[a].uv.x(), dv2 = vertices[c].uv.y() - vertices[a].uv.y();

            // Solves edge = T du + B dv for the triangle; multiplying by the determinant instead of
            // dividing weights each triangle by its UV area and keeps the sign of a mirrored one.
            const float determinant = du1 * dv2 - du2 * dv1;
            if (std::abs(determinant) < EPSILON)
                continue;
            const float       sign      = determinant < 0.0f ? -1.0f : 1.0f;
            const HM::Vector3 tangent   = (edge1 * dv2 - edge2 * dv1) * sign;
            const HM::Vector3 bitangent = (edge2 * du1 - edge1 * du2) * sign;
            for (const uint32_t vertex : { a, b, c })
            {
                tangents[vertex]   = tangents[vertex] + tangent;
                bitangents[vertex] = bitangents[vertex] + bitangent;
            }
        }

        for (size_t i = 0; i < vertices.size(); ++i)
        {
            LoadedVertexData& vertex       = vertices[i];
            const float       normalLength = Length(vertex.normal);
            if (normalLength < EPSILON)
            {
                vertex.tangent = HM::Vector4(1.0f, 0.0f, 0.0f, 1.0f);
                continue;
            }
            const HM::Vector3 normal = vertex.normal / normalLength;

            HM::Vector3 tangent = tangents[i] - normal * Dot(normal, tangents[i]);
            const float length  = Length(tangent);
            // No usable UVs, or a summed tangent along the normal.
            if (length < EPSILON || length <= 1e-4f * Length(tangents[i]))
            {
                const HM::Vector3 any = AnyTangent(normal);
                vertex.tangent        = HM::Vector4(any.x(), any.y(), any.z(), 1.0f);
                continue;
            }
            tangent = tangent / length;
            const float handedness = Dot(Cross(normal, tangent), bitangents[i]) < 0.0f ? -1.0f : 1.0f;
            vertex.tangent         = HM::Vector4(tangent.x(), tangent.y(), tangent.z(), handedness);
        }
    }
}
