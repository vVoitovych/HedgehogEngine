#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "ECS/api/System.hpp"
#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"

#include <vector>
#include <string>

namespace Scene
{
    class MeshSystem : public ECS::System
    {
    public:
        HEDGEHOG_ENGINE_API MeshSystem();

        HEDGEHOG_ENGINE_API void Update(ECS::ECS& ecs, ECS::Entity entity);
        HEDGEHOG_ENGINE_API void Update(ECS::ECS& ecs);

        HEDGEHOG_ENGINE_API bool ShouldUpdateMeshContainer() const;
        HEDGEHOG_ENGINE_API void MeshContainerUpdated();

        HEDGEHOG_ENGINE_API const std::vector<std::string>& GetMeshes() const;
        HEDGEHOG_ENGINE_API std::vector<ECS::Entity>        GetEntities() const;

        HEDGEHOG_ENGINE_API void AddMeshPath(const std::string& meshPath);
        HEDGEHOG_ENGINE_API void LoadMesh(ECS::ECS& ecs, ECS::Entity entity);

        HEDGEHOG_ENGINE_API static const std::string sDefaultMeshPath;

    private:
        void CheckMeshPath(MeshComponent& meshComponent, const std::string& fallbackPath);

    private:
        std::vector<std::string> m_MeshPaths;
        bool                     m_UpdateMeshContainer = false;
    };
}
