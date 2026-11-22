#include "HedgehogEngine/api/Navigation/NavigationGeometry.hpp"

#include "../ECS/PhysicsWorldTransform.hpp"

#include "HedgehogEngine/api/Containers/Mesh.hpp"
#include "HedgehogEngine/api/Containers/MeshContainer.hpp"
#include "HedgehogEngine/api/Containers/TerrainContainer.hpp"
#include "HedgehogEngine/api/ECS/components/ColliderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RenderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RigidBodyComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TerrainComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/PhysicsSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/TerrainSystem.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"
#include "HedgehogEngine/api/Terrain/TerrainGeometry.hpp"

#include "HedgehogSettings/api/LayerSettings.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace HedgehogEngine
{
    namespace
    {
        // Rings and segments of a tessellated sphere or capsule.
        constexpr uint32_t ROUND_STACKS   = 8; // even, so the equator is a ring
        constexpr uint32_t ROUND_SLICES   = 12;

        bool IsOnEditorLayer(ECS::ECS& ecs, ECS::Entity entity)
        {
            return ecs.HasComponent<RenderComponent>(entity) &&
                   ecs.GetComponent<RenderComponent>(entity).Layer == HedgehogSettings::LayerSettings::EDITOR_LAYER;
        }

        bool Moves(ECS::ECS& ecs, ECS::Entity entity)
        {
            return ecs.HasComponent<RigidBodyComponent>(entity) &&
                   ecs.GetComponent<RigidBodyComponent>(entity).BodyType != RigidBodyType::Static;
        }

        HM::Vector3 Transform(const HM::Matrix4x4& matrix, const HM::Vector3& point)
        {
            const HM::Vector4 p = matrix * HM::Vector4(point.x(), point.y(), point.z(), 1.0f);
            return HM::Vector3(p.x(), p.y(), p.z());
        }

        // Appends triangles over local points placed by place, offsetting their indices.
        template <typename Place>
        void Append(HN::NavMeshGeometry& out, const std::vector<HM::Vector3>& points, const std::vector<uint32_t>& indices,
                    Place&& place)
        {
            const auto first = static_cast<uint32_t>(out.Positions.size());
            for (const HM::Vector3& point : points)
                out.Positions.push_back(place(point));
            for (uint32_t index : indices)
                out.Indices.push_back(first + index);
        }

        // A box's corners and 12 triangles, counterclockwise seen from outside.
        void MakeBox(const HM::Vector3& half, std::vector<HM::Vector3>& points, std::vector<uint32_t>& indices)
        {
            for (uint32_t i = 0; i < 8; ++i)
                points.emplace_back(i & 1 ? half.x() : -half.x(), i & 2 ? half.y() : -half.y(), i & 4 ? half.z() : -half.z());
            const uint32_t faces[6][4] = { { 0, 2, 3, 1 }, { 4, 5, 7, 6 }, { 0, 1, 5, 4 },
                                           { 2, 6, 7, 3 }, { 0, 4, 6, 2 }, { 1, 3, 7, 5 } };
            for (const auto& f : faces)
                indices.insert(indices.end(), { f[0], f[1], f[2], f[0], f[2], f[3] });
        }

        // A sphere of radius (halfHeight 0) or a capsule along +Z whose hemispheres' centres are
        // halfHeight above and below its centre, counterclockwise seen from outside.
        void MakeRound(float radius, float halfHeight, std::vector<HM::Vector3>& points, std::vector<uint32_t>& indices)
        {
            constexpr float PI = std::numbers::pi_v<float>;
            // The equator twice, once for each hemisphere, so the capsule's side is a band between them.
            uint32_t rings = 0;
            for (uint32_t stack = 0; stack <= ROUND_STACKS; ++stack)
            {
                const uint32_t copies = stack == ROUND_STACKS / 2 ? 2 : 1;
                for (uint32_t copy = 0; copy < copies; ++copy, ++rings)
                {
                    const float theta  = PI * static_cast<float>(stack) / static_cast<float>(ROUND_STACKS);
                    const bool  upper  = stack < ROUND_STACKS / 2 || (stack == ROUND_STACKS / 2 && copy == 0);
                    const float offset = upper ? halfHeight : -halfHeight;
                    for (uint32_t slice = 0; slice < ROUND_SLICES; ++slice)
                    {
                        const float phi = 2.0f * PI * static_cast<float>(slice) / static_cast<float>(ROUND_SLICES);
                        points.emplace_back(radius * std::sin(theta) * std::cos(phi), radius * std::sin(theta) * std::sin(phi),
                                            radius * std::cos(theta) + offset);
                    }
                }
            }
            for (uint32_t ring = 0; ring + 1 < rings; ++ring)
                for (uint32_t slice = 0; slice < ROUND_SLICES; ++slice)
                {
                    const uint32_t next = (slice + 1) % ROUND_SLICES;
                    const uint32_t a0 = ring * ROUND_SLICES + slice, a1 = ring * ROUND_SLICES + next;
                    const uint32_t b0 = a0 + ROUND_SLICES, b1 = a1 + ROUND_SLICES;
                    indices.insert(indices.end(), { a0, b0, b1, a0, b1, a1 });
                }
        }

        void GatherColliders(ECS::ECS& ecs, HN::NavMeshGeometry& out)
        {
            const auto physics = ecs.HasSystem<PhysicsSystem>() ? ecs.GetSystem<PhysicsSystem>() : nullptr;
            if (!physics)
                return;
            std::vector<HM::Vector3> points;
            std::vector<uint32_t>    indices;
            for (const ECS::Entity entity : physics->GetEntities())
            {
                const ColliderComponent& collider = ecs.GetComponent<ColliderComponent>(entity);
                if (collider.IsTrigger || Moves(ecs, entity) || IsOnEditorLayer(ecs, entity))
                    continue;

                // As PhysicsSystem shapes the body: the size times the world scale, a round shape by
                // its largest axis, the centre scaled, then turned and moved.
                const WorldTransform world   = ComputeWorldTransform(ecs, entity);
                const HM::Vector3&   scale   = world.Scale;
                const HM::Vector3    s(std::abs(scale.x()), std::abs(scale.y()), std::abs(scale.z()));
                const float          largest = std::max({ s.x(), s.y(), s.z() });
                const HM::Vector3    centre(collider.Center.x() * s.x(), collider.Center.y() * s.y(), collider.Center.z() * s.z());
                const HM::Vector4&   column  = world.Matrix[3];
                const HM::Vector3    position(column.x(), column.y(), column.z());

                points.clear();
                indices.clear();
                switch (collider.Shape)
                {
                case ColliderShape::Box:
                    MakeBox(HM::Vector3(collider.Size.x() * 0.5f * s.x(), collider.Size.y() * 0.5f * s.y(),
                                        collider.Size.z() * 0.5f * s.z()),
                            points, indices);
                    break;
                case ColliderShape::Sphere:
                    MakeRound(collider.Radius * largest, 0.0f, points, indices);
                    break;
                case ColliderShape::Capsule:
                    MakeRound(collider.Radius * largest, std::max(0.0f, collider.Height * 0.5f - collider.Radius) * largest,
                              points, indices);
                    break;
                }
                Append(out, points, indices,
                       [&](const HM::Vector3& local) { return position + world.Rotation * (centre + local); });
            }
        }

        void GatherTerrains(ECS::ECS& ecs, const ResourceCatalog& catalog, HN::NavMeshGeometry& out)
        {
            const auto terrains = ecs.HasSystem<TerrainSystem>() ? ecs.GetSystem<TerrainSystem>() : nullptr;
            if (!terrains)
                return;
            const TerrainContainer& container = catalog.GetTerrainContainer();
            for (const ECS::Entity entity : terrains->GetEntities())
            {
                const TerrainComponent& terrain = ecs.GetComponent<TerrainComponent>(entity);
                if (!terrain.Enabled || terrain.TerrainIndex < 0 ||
                    static_cast<size_t>(terrain.TerrainIndex) >= container.GetTerrainCount() ||
                    terrain.RenderLayer == HedgehogSettings::LayerSettings::EDITOR_LAYER)
                    continue;
                const TerrainMesh    mesh  = BuildTerrainMesh(container.GetTerrain(static_cast<size_t>(terrain.TerrainIndex)));
                const WorldTransform world = ComputeWorldTransform(ecs, entity);
                Append(out, mesh.Positions, mesh.Indices, [&](const HM::Vector3& local) { return Transform(world.Matrix, local); });
            }
        }

        void GatherRenderMeshes(ECS::ECS& ecs, const ResourceCatalog& catalog, HN::NavMeshGeometry& out)
        {
            const auto meshes = ecs.HasSystem<MeshSystem>() ? ecs.GetSystem<MeshSystem>() : nullptr;
            if (!meshes)
                return;
            const MeshContainer& container = catalog.GetMeshContainer();
            for (const ECS::Entity entity : meshes->GetEntities())
            {
                const MeshComponent& component = ecs.GetComponent<MeshComponent>(entity);
                if (!component.MeshIndex || *component.MeshIndex >= container.GetMeshCount() ||
                    !ecs.HasComponent<TransformComponent>(entity) || Moves(ecs, entity) || IsOnEditorLayer(ecs, entity))
                    continue;
                const Mesh&          mesh   = container.GetMesh(static_cast<size_t>(*component.MeshIndex));
                const HM::Matrix4x4& matrix = ecs.GetComponent<TransformComponent>(entity).ObjMatrix;
                Append(out, mesh.GetPositions(), mesh.GetIndices(), [&](const HM::Vector3& local) { return Transform(matrix, local); });
            }
        }
    }

    HN::NavMeshGeometry GatherNavigationGeometry(ECS::ECS& ecs, const ResourceCatalog& catalog,
                                                 const NavigationGeometryOptions& options)
    {
        HN::NavMeshGeometry geometry;
        GatherColliders(ecs, geometry);
        GatherTerrains(ecs, catalog, geometry);
        if (options.IncludeRenderMeshes)
            GatherRenderMeshes(ecs, catalog, geometry);
        return geometry;
    }
}
