#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "HedgehogNavigation/api/NavMesh.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/System.hpp"

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace FS
{
    class FileSystemManager;
}

namespace HedgehogEngine
{
    // Loads the scene's navmesh and answers path queries on it. Its entities are NavMeshComponent +
    // TransformComponent; the first enabled one (by entity id) naming a file is the one loaded, by
    // FS::MakeAssetKey, through the FileSystemManager service. Each frame (SystemPhase::Late) it
    // notices a changed component at once and, at most once per POLL_INTERVAL, a file whose write time
    // has moved on, which it reads again; a file that does not read is logged once, [Navigation]
    // <path>: ..., and not tried again until it changes.
    class NavigationSystem : public ECS::System
    {
    public:
        static constexpr std::chrono::seconds POLL_INTERVAL{ 1 };

        HEDGEHOG_ENGINE_API void OnRegister(ECS::ECS& ecs) override;
        [[nodiscard]] HEDGEHOG_ENGINE_API ECS::SystemPhase GetPhase() const override;
        HEDGEHOG_ENGINE_API void OnFrame(ECS::ECS& ecs, const ECS::FrameContext& ctx) override;

        // The frame's work, with the time a test may move on.
        HEDGEHOG_ENGINE_API void Update(ECS::ECS& ecs, std::chrono::steady_clock::time_point now);

        [[nodiscard]] HEDGEHOG_ENGINE_API bool HasNavMesh() const;
        // The asset key of the navmesh loaded or tried, empty for none.
        [[nodiscard]] HEDGEHOG_ENGINE_API const std::string& GetNavMeshKey() const;

        HEDGEHOG_ENGINE_API HN::NavPathStatus FindPath(const HM::Vector3& start, const HM::Vector3& end,
                                                       std::vector<HM::Vector3>& outCorners);
        [[nodiscard]] HEDGEHOG_ENGINE_API std::optional<HM::Vector3> SamplePosition(const HM::Vector3& point, float maxDistance);
        [[nodiscard]] HEDGEHOG_ENGINE_API std::optional<HN::NavRayHit> Raycast(const HM::Vector3& start, const HM::Vector3& end);
        // For drawing.
        [[nodiscard]] HEDGEHOG_ENGINE_API const HN::NavMesh& GetNavMesh() const;

    private:
        void Load(const std::string& key);
        [[nodiscard]] std::filesystem::file_time_type GetWriteTime(const std::string& key) const;

        const FS::FileSystemManager*          m_Files = nullptr;
        HN::NavMesh                           m_NavMesh;
        std::string                           m_Key;
        std::filesystem::file_time_type       m_WriteTime{};
        std::chrono::steady_clock::time_point m_LastPoll{};
    };
}
