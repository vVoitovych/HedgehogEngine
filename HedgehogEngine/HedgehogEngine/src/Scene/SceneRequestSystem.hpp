#pragma once

#include "HedgehogEngine/api/Async/BackgroundWorker.hpp"

#include "ECS/api/System.hpp"

#include "yaml-cpp/yaml.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
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
    // An asynchronous request is read on the system's background worker first: the scene's YAML
    // parsed and every mesh, texture and environment it reaches decoded into ContentLoader's
    // prefetch store; the swap waits for the first frame after that, and the loads it makes take
    // from the store. The worker never touches the ECS.
    class SceneRequestSystem : public ECS::System
    {
    public:
        SceneRequestSystem(SceneManager& scenes, EventBus& eventBus, const SaveGameManager& saves,
                           const FS::FileSystemManager& fileSystem);

        // The last request of a frame wins, an asynchronous one still loading included (it is
        // cancelled). loadRequestCount is the save manager's count when the request was made, to
        // tell whether a save load was requested after it.
        void Request(const std::string& virtualPath, uint64_t loadRequestCount);
        void RequestAsync(const std::string& virtualPath, uint64_t loadRequestCount);
        // Drops a pending request, cancelling a background load and dropping what it prefetched.
        void Clear();
        // An asynchronous load's progress, 0 to 1 (the share of its assets decoded); nullopt with none.
        [[nodiscard]] std::optional<float> GetAsyncProgress() const;

        [[nodiscard]] ECS::SystemPhase GetPhase() const override;
        void OnFrame(ECS::ECS& ecs, const ECS::FrameContext& ctx) override;
        void OnUnregister(ECS::ECS& ecs) override;

    private:
        struct PendingScene
        {
            std::string Path;
            uint64_t    LoadRequestCount = 0;
        };

        // Shared with the job; the main thread reads Document and Error once the job is done.
        struct AsyncLoad
        {
            PendingScene                          Request;
            BackgroundWorker::JobId               Job = 0;
            std::atomic<uint32_t>                 Done{ 0 };
            std::atomic<uint32_t>                 Total{ 0 };
            std::atomic<bool>                     Cancelled{ false };
            std::chrono::steady_clock::time_point Start;
            double                                BackgroundMs = 0.0;
            YAML::Node                            Document; // parsed by the job, read once it is done
            std::string                           Error;
        };

        void CancelAsync();
        // Replaces the world with pending's scene: its file read here, or document when given.
        bool Swap(const PendingScene& pending, const YAML::Node* document);
        static void RunAsyncLoad(AsyncLoad& load, const FS::FileSystemManager& fileSystem, std::stop_token stop);

        SceneManager&                m_Scenes;
        EventBus&                    m_EventBus;
        const SaveGameManager&       m_Saves;
        const FS::FileSystemManager& m_FileSystem;
        std::optional<PendingScene>  m_Pending;
        std::shared_ptr<AsyncLoad>   m_Async;
        bool                         m_ClearPrefetched = false; // the frame after a swap drops what no load took
        BackgroundWorker             m_Worker;                  // last, so it is joined before the rest goes
    };
}
