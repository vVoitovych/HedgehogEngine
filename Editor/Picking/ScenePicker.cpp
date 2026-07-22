#include "ScenePicker.hpp"

#include "HedgehogEngine/api/Engine.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"
#include "HedgehogEngine/api/Containers/MeshContainer.hpp"
#include "HedgehogEngine/api/Containers/Mesh.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RenderComponent.hpp"

#include "HedgehogCommon/api/Frame/FrameData.hpp"

#include "ECS/api/ECS.hpp"

#include "HedgehogMath/api/Matrix.hpp"
#include "HedgehogMath/api/Vector.hpp"
#include "HedgehogMath/api/AABB.hpp"

#include <cmath>
#include <limits>

namespace Editor::Picking
{
namespace
{
    HM::Vector3 TransformPoint(const HM::Matrix4x4& m, const HM::Vector3& p)
    {
        const HM::Vector4 r = HM::Vector4(p.x(), p.y(), p.z(), 1.0f) * m;
        return HM::Vector3(r.x(), r.y(), r.z());
    }

    HM::Vector3 TransformDir(const HM::Matrix4x4& m, const HM::Vector3& d)
    {
        const HM::Vector4 r = HM::Vector4(d.x(), d.y(), d.z(), 0.0f) * m;
        return HM::Vector3(r.x(), r.y(), r.z());
    }
}

    std::optional<ECS::Entity> PickEntity(HedgehogEngine::Engine& context,
                                          float pixelX, float pixelY,
                                          float viewWidth, float viewHeight)
    {
        if (viewWidth <= 0.0f || viewHeight <= 0.0f)
            return std::nullopt;

        auto&       engineContext = context.GetEngineContext();
        // Use the frame's camera matrices — exactly what rendered the scene view this frame.
        const auto& frameCamera   = engineContext.GetFrameData().Camera;

        const HM::Matrix4x4 viewProj = frameCamera.Proj * frameCamera.View;
        bool invertible = false;
        const HM::Matrix4x4 invVP = viewProj.Inverse(invertible);
        if (!invertible)
            return std::nullopt;

        // Pixel (top-left origin) → NDC. The Vulkan framebuffer top maps to ndc.y = -1, which is how
        // the scene texture is displayed in the panel, so the same 2*u-1 mapping applies to both axes.
        const float ndcX = 2.0f * ((pixelX + 0.5f) / viewWidth)  - 1.0f;
        const float ndcY = 2.0f * ((pixelY + 0.5f) / viewHeight) - 1.0f;

        const HM::Vector3 nearPoint = HM::UnprojectNdc(invVP, HM::Vector3(ndcX, ndcY, 0.0f));
        const HM::Vector3 farPoint  = HM::UnprojectNdc(invVP, HM::Vector3(ndcX, ndcY, 1.0f));
        const HM::Vector3 rayOrigin = nearPoint;
        const HM::Vector3 rayDir    = (farPoint - nearPoint).Normalize();

        auto*       meshSystem = engineContext.GetMeshSystem();
        const auto& meshes     = engineContext.GetResourceCatalog().GetMeshContainer();
        auto&       ecs        = engineContext.GetECS();
        if (meshSystem == nullptr)
            return std::nullopt;

        std::optional<ECS::Entity> hitEntity;
        float nearestDist = std::numeric_limits<float>::infinity();

        for (ECS::Entity entity : meshSystem->GetEntities())
        {
            if (!ecs.HasComponent<HedgehogEngine::TransformComponent>(entity))
                continue;

            const auto& meshComp = ecs.GetComponent<HedgehogEngine::MeshComponent>(entity);
            if (!meshComp.MeshIndex.has_value())
                continue;
            const uint64_t meshIndex = meshComp.MeshIndex.value();
            if (meshIndex >= meshes.GetMeshCount())
                continue;

            if (ecs.HasComponent<HedgehogEngine::RenderComponent>(entity) &&
                !ecs.GetComponent<HedgehogEngine::RenderComponent>(entity).IsVisible)
                continue;

            const HM::Matrix4x4& world = ecs.GetComponent<HedgehogEngine::TransformComponent>(entity).ObjMatrix;
            bool worldInvertible = false;
            const HM::Matrix4x4 invWorld = world.Inverse(worldInvertible);
            if (!worldInvertible)
                continue;

            // Test in the mesh's local space (equivalent to an OBB test in world space).
            const HM::Vector3 localOrigin = TransformPoint(invWorld, rayOrigin);
            const HM::Vector3 localDir    = TransformDir(invWorld, rayDir);

            float tLocal = 0.0f;
            if (!meshes.GetMesh(meshIndex).GetLocalBounds().IntersectRay(localOrigin, localDir, tLocal))
                continue;

            // Rank hits by world-space distance so entities of different scales compare fairly.
            const HM::Vector3 localHit = localOrigin + localDir * tLocal;
            const HM::Vector3 worldHit = TransformPoint(world, localHit);
            const HM::Vector3 delta    = worldHit - rayOrigin;
            const float dist = std::sqrt(delta.x() * delta.x() + delta.y() * delta.y() + delta.z() * delta.z());

            if (dist < nearestDist)
            {
                nearestDist = dist;
                hitEntity   = entity;
            }
        }

        return hitEntity;
    }
}
