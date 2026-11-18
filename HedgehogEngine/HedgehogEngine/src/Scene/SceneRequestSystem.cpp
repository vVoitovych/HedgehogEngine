#include "SceneRequestSystem.hpp"

#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/SceneEvents.hpp"
#include "HedgehogEngine/api/Save/SaveGameManager.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "ECS/api/FrameContext.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "Logger/api/Logger.hpp"

#include <utility>

namespace HedgehogEngine
{
    SceneRequestSystem::SceneRequestSystem(SceneManager& scenes, EventBus& eventBus, const SaveGameManager& saves,
                                           const FS::FileSystemManager& fileSystem)
        : m_Scenes(scenes)
        , m_EventBus(eventBus)
        , m_Saves(saves)
        , m_FileSystem(fileSystem)
    {
    }

    void SceneRequestSystem::Request(const std::string& virtualPath, uint64_t loadRequestCount)
    {
        m_Pending = PendingScene{virtualPath, loadRequestCount};
    }

    void SceneRequestSystem::Clear()
    {
        m_Pending.reset();
    }

    ECS::SystemPhase SceneRequestSystem::GetPhase() const
    {
        return ECS::SystemPhase::Simulation;
    }

    void SceneRequestSystem::OnFrame(ECS::ECS& /*ecs*/, const ECS::FrameContext& ctx)
    {
        if (!m_Pending || ctx.Mode != ECS::PlayMode::Playing)
            return;

        const PendingScene pending = std::exchange(m_Pending, std::nullopt).value();
        // The later save load has already run in SaveRequestSystem; if it failed, neither change
        // applies, which its own error explains.
        if (m_Saves.GetLoadRequestCount() != pending.LoadRequestCount)
        {
            LOGWARNING("[Scene] The request to load " + pending.Path
                       + " is dropped: a save load was requested after it.");
            return;
        }

        const std::optional<std::filesystem::path> physical = m_FileSystem.ResolvePhysical(pending.Path);
        if (!physical)
        {
            LOGERROR("[Scene] " + pending.Path + " cannot be resolved to a file; it is not loaded.");
            return;
        }

        // LoadScene clears the world before it parses the file, so a file that does not load
        // would leave the game empty: keep the running scene to put back.
        const SceneSnapshot running = m_Scenes.CaptureSnapshot();
        if (!m_Scenes.LoadScene(physical->string()))
        {
            if (m_Scenes.RestoreSnapshot(running))
                LOGERROR("[Scene] " + pending.Path + " could not be loaded; the running scene is kept.");
            else
                LOGERROR("[Scene] " + pending.Path + " could not be loaded, nor the running scene put back.");
            return;
        }
        m_EventBus.Publish(SceneLoadedEvent{m_Scenes.GetScenePath()});
    }
}
