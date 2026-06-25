#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "Logger/api/Logger.hpp"
#include "DialogueWindows/api/MeshDialogue.hpp"

#include <algorithm>
#include <string_view>

namespace HedgehogEngine
{
    const std::string MeshSystem::sDefaultMeshPath = "Models/Default/cube.obj";

    MeshSystem::MeshSystem()
    {
        AddMeshPath("Models/Default/cube.obj");
        AddMeshPath("Models/Default/sphere.obj");
    }

    void MeshSystem::Update(ECS::ECS& ecs, ECS::Entity entity,
                             const FS::FileSystemManager& fileSystem)
    {
        auto& meshComponent = ecs.GetComponent<MeshComponent>(entity);
        if (meshComponent.m_MeshIndex.has_value())
        {
            size_t index = meshComponent.m_MeshIndex.value();
            if (m_MeshPaths[index] != meshComponent.m_MeshPath)
                CheckMeshPath(meshComponent, meshComponent.m_CachedMeshPath, fileSystem);
        }
        else
        {
            CheckMeshPath(meshComponent, sDefaultMeshPath, fileSystem);
        }
    }

    void MeshSystem::Update(ECS::ECS& ecs, const FS::FileSystemManager& fileSystem)
    {
        for (auto& entity : m_Entities)
        {
            auto& meshComponent = ecs.GetComponent<MeshComponent>(entity);
            if (meshComponent.m_MeshIndex.has_value())
            {
                size_t index = meshComponent.m_MeshIndex.value();
                if (m_MeshPaths[index] != meshComponent.m_MeshPath)
                    CheckMeshPath(meshComponent, meshComponent.m_CachedMeshPath, fileSystem);
            }
            else
            {
                CheckMeshPath(meshComponent, sDefaultMeshPath, fileSystem);
            }
        }
    }

    bool MeshSystem::ShouldUpdateMeshContainer() const
    {
        return m_UpdateMeshContainer;
    }

    void MeshSystem::MeshContainerUpdated()
    {
        m_UpdateMeshContainer = false;
    }

    const std::vector<std::string>& MeshSystem::GetMeshes() const
    {
        return m_MeshPaths;
    }

    std::vector<ECS::Entity> MeshSystem::GetEntities() const
    {
        return m_Entities;
    }

    void MeshSystem::AddMeshPath(const std::string& meshPath)
    {
        m_MeshPaths.push_back(meshPath);
    }

    void MeshSystem::LoadMesh(ECS::ECS& ecs, ECS::Entity entity,
                               const FS::FileSystemManager& fileSystem)
    {
        char* path = DialogueWindows::MeshOpenDialogue();
        if (path == nullptr)
            return;

        const auto virtualPath = fileSystem.ToVirtualPath(path);
        if (!virtualPath)
        {
            LOGERROR("MeshSystem::LoadMesh: path is not under any registered mount (path: ", path, ")");
            return;
        }

        // Strip "assets://" prefix; m_MeshPath stores the relative path.
        constexpr std::string_view k_AssetsPrefix = "assets://";
        const std::string relatedPath = virtualPath->substr(k_AssetsPrefix.size());

        auto& meshComponent      = ecs.GetComponent<MeshComponent>(entity);
        auto  prevMeshPath       = meshComponent.m_MeshPath;
        meshComponent.m_MeshPath = relatedPath;
        AddMeshPath(relatedPath);
        CheckMeshPath(meshComponent, prevMeshPath, fileSystem);
    }

    void MeshSystem::CheckMeshPath(MeshComponent& meshComponent, const std::string& fallbackPath,
                                    const FS::FileSystemManager& fileSystem)
    {
        auto it = std::find(m_MeshPaths.begin(), m_MeshPaths.end(), meshComponent.m_MeshPath);
        if (it != m_MeshPaths.end())
        {
            uint64_t newIndex              = static_cast<uint64_t>(it - m_MeshPaths.begin());
            meshComponent.m_MeshIndex      = newIndex;
            meshComponent.m_CachedMeshPath = meshComponent.m_MeshPath;
        }
        else
        {
            if (fileSystem.Exists("assets://" + meshComponent.m_MeshPath))
            {
                meshComponent.m_CachedMeshPath = meshComponent.m_MeshPath;
                meshComponent.m_MeshIndex      = m_MeshPaths.size();
                m_MeshPaths.push_back(meshComponent.m_MeshPath);
                m_UpdateMeshContainer = true;
            }
            else
            {
                LOGERROR("Wrong file path: ", meshComponent.m_MeshPath);
                meshComponent.m_MeshPath = fallbackPath;
            }
        }
    }
}
