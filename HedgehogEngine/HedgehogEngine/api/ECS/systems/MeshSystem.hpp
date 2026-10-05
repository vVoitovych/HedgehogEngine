#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "ECS/api/System.hpp"
#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <vector>
#include <string>

namespace HedgehogEngine
{
    class ResourceCatalog;

    // Lists the mesh files MeshComponents name. In the Sync phase it has the ResourceCatalog
    // service (found in OnRegister; nothing without one) load the ones listed since the last frame.
    class MeshSystem : public ECS::System
    {
    public:
        HEDGEHOG_ENGINE_API MeshSystem();

        HEDGEHOG_ENGINE_API void OnRegister(ECS::ECS& ecs) override;
        HEDGEHOG_ENGINE_API void OnUnregister(ECS::ECS& ecs) override;
        [[nodiscard]] HEDGEHOG_ENGINE_API ECS::SystemPhase GetPhase() const override;
        HEDGEHOG_ENGINE_API void OnFrame(ECS::ECS& ecs, const ECS::FrameContext& ctx) override;

        HEDGEHOG_ENGINE_API void Update(ECS::ECS& ecs, ECS::Entity entity,
                                          const FS::FileSystemManager& fileSystem);
        HEDGEHOG_ENGINE_API void Update(ECS::ECS& ecs,
                                        const FS::FileSystemManager& fileSystem);

        HEDGEHOG_ENGINE_API bool ShouldUpdateMeshContainer() const;
        HEDGEHOG_ENGINE_API void MeshContainerUpdated();

        HEDGEHOG_ENGINE_API const std::vector<std::string>& GetMeshes() const;
        HEDGEHOG_ENGINE_API std::vector<ECS::Entity>        GetEntities() const;

        HEDGEHOG_ENGINE_API void AddMeshPath(const std::string& meshPath);
        // Takes the relative mesh path (without "assets://" prefix).
        // The caller is responsible for opening any file dialog and resolving the path.
        HEDGEHOG_ENGINE_API void LoadMesh(ECS::ECS& ecs, ECS::Entity entity,
                                          const std::string& relativePath);

        HEDGEHOG_ENGINE_API static const std::string sDefaultMeshPath;

    private:
        void CheckMeshPath(MeshComponent& meshComponent, const std::string& fallbackPath,
                           const FS::FileSystemManager& fileSystem);

    private:
        std::vector<std::string> m_MeshPaths;
        bool                     m_UpdateMeshContainer = false;
        ResourceCatalog*         m_Catalog             = nullptr;
    };
}
