#include "HedgehogEngine/api/ECS/systems/NavigationSystem.hpp"

#include "HedgehogEngine/api/ECS/components/NavMeshComponent.hpp"
#include "HedgehogEngine/api/Navigation/NavMeshFile.hpp"

#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/api/PathUtils.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>

namespace HedgehogEngine
{
    void NavigationSystem::OnRegister(ECS::ECS& ecs)
    {
        m_Files = ecs.GetServices().Find<FS::FileSystemManager>();
    }

    ECS::SystemPhase NavigationSystem::GetPhase() const
    {
        return ECS::SystemPhase::Late;
    }

    void NavigationSystem::OnFrame(ECS::ECS& ecs, const ECS::FrameContext&)
    {
        Update(ecs, std::chrono::steady_clock::now());
    }

    void NavigationSystem::Update(ECS::ECS& ecs, std::chrono::steady_clock::time_point now)
    {
        // The first enabled component naming a file, by entity id.
        ECS::Entity chosen = ECS::INVALID_ENTITY;
        for (const ECS::Entity entity : GetEntities())
        {
            const NavMeshComponent& component = ecs.GetComponent<NavMeshComponent>(entity);
            if (component.Enabled && !component.NavMesh.empty() && (chosen == ECS::INVALID_ENTITY || entity < chosen))
                chosen = entity;
        }
        const std::string key = chosen == ECS::INVALID_ENTITY
                                    ? std::string()
                                    : FS::MakeAssetKey(ecs.GetComponent<NavMeshComponent>(chosen).NavMesh);

        if (key != m_Key)
        {
            m_Key = key;
            m_LastPoll = now;
            if (m_Key.empty())
                m_NavMesh.Unload();
            else
                Load(m_Key);
        }
        else if (!m_Key.empty() && m_Files && now - m_LastPoll >= POLL_INTERVAL)
        {
            m_LastPoll = now;
            if (GetWriteTime(m_Key) != m_WriteTime)
                Load(m_Key);
        }

        for (const ECS::Entity entity : GetEntities())
            ecs.GetComponent<NavMeshComponent>(entity).Loaded = entity == chosen && m_NavMesh.IsLoaded();
    }

    std::filesystem::file_time_type NavigationSystem::GetWriteTime(const std::string& key) const
    {
        std::error_code error;
        if (const auto physical = m_Files->ResolvePhysical(FS::ToAssetVirtualPath(key)))
        {
            const auto time = std::filesystem::last_write_time(*physical, error);
            if (!error)
                return time;
        }
        return std::filesystem::file_time_type::min();
    }

    void NavigationSystem::Load(const std::string& key)
    {
        m_NavMesh.Unload();
        if (!m_Files)
            return;
        m_WriteTime            = GetWriteTime(key);
        const std::string path = FS::ToAssetVirtualPath(key);
        if (!m_Files->Exists(path))
        {
            LOGERROR("[Navigation] " + path + ": the file does not exist.");
            return;
        }
        const auto bytes = m_Files->ReadFile(path);
        if (!bytes)
        {
            LOGERROR("[Navigation] " + path + ": the file cannot be read.");
            return;
        }
        std::vector<uint8_t> raw(bytes->size());
        std::transform(bytes->begin(), bytes->end(), raw.begin(), [](std::byte b) { return static_cast<uint8_t>(b); });
        const NavMeshFileReadResult file = ReadNavMeshFile(raw);
        if (!file.Error.empty())
        {
            LOGERROR("[Navigation] " + path + ": " + file.Error + ".");
            return;
        }
        if (m_NavMesh.Load(file.Data))
            LOGINFO("[Navigation] Loaded " + path + ".");
    }

    bool NavigationSystem::HasNavMesh() const
    {
        return m_NavMesh.IsLoaded();
    }

    const std::string& NavigationSystem::GetNavMeshKey() const
    {
        return m_Key;
    }

    HN::NavPathStatus NavigationSystem::FindPath(const HM::Vector3& start, const HM::Vector3& end,
                                                 std::vector<HM::Vector3>& outCorners)
    {
        return m_NavMesh.FindPath(start, end, outCorners);
    }

    std::optional<HM::Vector3> NavigationSystem::SamplePosition(const HM::Vector3& point, float maxDistance)
    {
        return m_NavMesh.SamplePosition(point, maxDistance);
    }

    std::optional<HN::NavRayHit> NavigationSystem::Raycast(const HM::Vector3& start, const HM::Vector3& end)
    {
        return m_NavMesh.Raycast(start, end);
    }

    const HN::NavMesh& NavigationSystem::GetNavMesh() const
    {
        return m_NavMesh;
    }
}
