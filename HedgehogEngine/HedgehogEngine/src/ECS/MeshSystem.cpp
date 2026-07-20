#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "Logger/api/Logger.hpp"

#include <algorithm>

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
        if (meshComponent.MeshIndex.has_value())
        {
            size_t index = meshComponent.MeshIndex.value();
            if (m_MeshPaths[index] != meshComponent.MeshPath)
                CheckMeshPath(meshComponent, meshComponent.CachedMeshPath, fileSystem);
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
            if (meshComponent.MeshIndex.has_value())
            {
                size_t index = meshComponent.MeshIndex.value();
                if (m_MeshPaths[index] != meshComponent.MeshPath)
                    CheckMeshPath(meshComponent, meshComponent.CachedMeshPath, fileSystem);
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

    void MeshSystem::LoadMesh(ECS::ECS& ecs, ECS::Entity entity, const std::string& relativePath)
    {
        auto& meshComponent      = ecs.GetComponent<MeshComponent>(entity);
        meshComponent.MeshPath = relativePath;

        auto it = std::find(m_MeshPaths.begin(), m_MeshPaths.end(), relativePath);
        if (it == m_MeshPaths.end())
        {
            meshComponent.MeshIndex = static_cast<uint64_t>(m_MeshPaths.size());
            m_MeshPaths.push_back(relativePath);
            m_UpdateMeshContainer = true;
        }
        else
        {
            meshComponent.MeshIndex = static_cast<uint64_t>(it - m_MeshPaths.begin());
        }
        meshComponent.CachedMeshPath = relativePath;
    }

    void MeshSystem::CheckMeshPath(MeshComponent& meshComponent, const std::string& fallbackPath,
                                    const FS::FileSystemManager& fileSystem)
    {
        auto it = std::find(m_MeshPaths.begin(), m_MeshPaths.end(), meshComponent.MeshPath);
        if (it != m_MeshPaths.end())
        {
            uint64_t newIndex              = static_cast<uint64_t>(it - m_MeshPaths.begin());
            meshComponent.MeshIndex      = newIndex;
            meshComponent.CachedMeshPath = meshComponent.MeshPath;
        }
        else
        {
            if (fileSystem.Exists("assets://" + meshComponent.MeshPath))
            {
                meshComponent.CachedMeshPath = meshComponent.MeshPath;
                meshComponent.MeshIndex      = m_MeshPaths.size();
                m_MeshPaths.push_back(meshComponent.MeshPath);
                m_UpdateMeshContainer = true;
            }
            else
            {
                LOGERROR("Wrong file path: ", meshComponent.MeshPath);
                meshComponent.MeshPath = fallbackPath;
            }
        }
    }
}
