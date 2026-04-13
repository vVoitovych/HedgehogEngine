#pragma once

#include "ECS/ECS.hpp"
#include "ECS/Entity.hpp"
#include "Scene/SceneComponents/MeshComponent.hpp"

#include <vector>
#include <string>

namespace Scene
{
    class MeshSystem : public ECS::System
    {
    public:
        MeshSystem();
        void Update(ECS::ECS& ecs, ECS::Entity entity);
        void Update(ECS::ECS& ecs);

        bool ShouldUpdateMeshContainer() const;
        void MeshContainerUpdated();
        const std::vector<std::string>& GetMeshes() const;
        std::vector<ECS::Entity> GetEntities() const;

        void AddMeshPath(const std::string& meshPath);
        void LoadMesh(ECS::ECS& ecs, ECS::Entity entity);

    public:
        static const std::string sDefaultMeshPath;

    private:
        void CheckMeshPath(MeshComponent& meshComponent, const std::string& fallbackPath);

    private:
        std::vector<std::string> m_MeshPaths;
        bool                     m_UpdateMeshContainer = false;
    };
}
