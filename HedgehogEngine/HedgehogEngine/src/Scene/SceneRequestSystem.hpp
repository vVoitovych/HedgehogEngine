#pragma once

#include "ECS/api/System.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace FS
{
    class FileSystemManager;
}

namespace HedgehogEngine
{
    class EventBus;
    class SaveGameManager;
    class SceneManager;

    // Replaces the world with the scene EngineContext::RequestSceneLoad queued, in the Simulation
    // phase's OnFrame, so it runs after every script hook and after the save requests of the same
    // frame (SaveRequestSystem is registered first): a save sees the old world. Only while Playing.
    class SceneRequestSystem : public ECS::System
    {
    public:
        SceneRequestSystem(SceneManager& scenes, EventBus& eventBus, const SaveGameManager& saves,
                           const FS::FileSystemManager& fileSystem);

        // The last request of a frame wins. loadRequestCount is the save manager's count when the
        // request was made, to tell whether a save load was requested after it.
        void Request(const std::string& virtualPath, uint64_t loadRequestCount);
        void Clear();

        [[nodiscard]] ECS::SystemPhase GetPhase() const override;
        void OnFrame(ECS::ECS& ecs, const ECS::FrameContext& ctx) override;

    private:
        struct PendingScene
        {
            std::string Path;
            uint64_t    LoadRequestCount = 0;
        };

        SceneManager&                m_Scenes;
        EventBus&                    m_EventBus;
        const SaveGameManager&       m_Saves;
        const FS::FileSystemManager& m_FileSystem;
        std::optional<PendingScene>  m_Pending;
    };
}
