#include "SceneRequestSystem.hpp"

#include "HedgehogEngine/api/Assets/EngineAssetDependencies.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/SceneEvents.hpp"
#include "HedgehogEngine/api/Save/SaveGameManager.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "ContentLoader/api/AssetPrefetch.hpp"
#include "EcsSerialization/api/Assets/AssetDependencyCollector.hpp"

#include "ECS/api/FrameContext.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <utility>
#include <vector>

namespace HedgehogEngine
{
    namespace
    {
        enum class PrefetchKind
        {
            None,
            Mesh,
            Texture,
            Environment,
        };

        // The assets a scene's loads read through ContentLoader, by extension.
        PrefetchKind KindOf(const std::string& path)
        {
            std::string extension = std::filesystem::path(path).extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (extension == ".obj" || extension == ".gltf" || extension == ".glb")
                return PrefetchKind::Mesh;
            if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".tga" ||
                extension == ".bmp")
                return PrefetchKind::Texture;
            if (extension == ".hdr")
                return PrefetchKind::Environment;
            return PrefetchKind::None;
        }

        double MillisecondsSince(std::chrono::steady_clock::time_point start)
        {
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        }
    }

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
        CancelAsync();
        m_Pending = PendingScene{ virtualPath, loadRequestCount };
    }

    void SceneRequestSystem::RequestAsync(const std::string& virtualPath, uint64_t loadRequestCount)
    {
        CancelAsync();
        m_Pending.reset();
        auto load     = std::make_shared<AsyncLoad>();
        load->Request = PendingScene{ virtualPath, loadRequestCount };
        load->Start   = std::chrono::steady_clock::now();
        // The job keeps the load alive; the file system outlives the worker, which is joined first.
        load->Job = m_Worker.Submit([load, &files = m_FileSystem](std::stop_token stop)
                                    { RunAsyncLoad(*load, files, stop); });
        m_Async = std::move(load);
    }

    void SceneRequestSystem::Clear()
    {
        m_Pending.reset();
        CancelAsync();
    }

    void SceneRequestSystem::CancelAsync()
    {
        if (!m_Async)
            return;
        // Waits for the job (at most the asset it is decoding), so nothing is prefetched after the clear.
        m_Async->Cancelled = true;
        m_Worker.Wait(m_Async->Job);
        m_Async.reset();
        ContentLoader::ClearPrefetched();
    }

    std::optional<float> SceneRequestSystem::GetAsyncProgress() const
    {
        if (!m_Async)
            return std::nullopt;
        if (m_Worker.IsDone(m_Async->Job))
            return 1.0f;
        const uint32_t total = m_Async->Total;
        return total == 0 ? 0.0f : std::min(1.0f, static_cast<float>(m_Async->Done) / static_cast<float>(total));
    }

    void SceneRequestSystem::RunAsyncLoad(AsyncLoad& load, const FS::FileSystemManager& fileSystem,
                                          std::stop_token stop)
    {
        const std::string& path = load.Request.Path;
        const auto         text = fileSystem.ReadTextFile(path);
        if (!text)
        {
            load.Error = "it cannot be read";
            return;
        }
        try
        {
            load.Document = YAML::Load(*text);
        }
        catch (const YAML::Exception& e)
        {
            load.Error = std::string("it does not parse: ") + e.what();
            return;
        }

        EcsSerialization::AssetDependencyCollector collector;
        RegisterEngineAssetDependencies(collector);
        std::vector<std::string> assets;
        for (const std::string& asset : collector.CollectScene(path, fileSystem).Assets)
            if (KindOf(asset) != PrefetchKind::None)
                assets.push_back(asset);
        load.Total = static_cast<uint32_t>(assets.size());

        for (const std::string& asset : assets)
        {
            if (load.Cancelled || stop.stop_requested())
                return;
            switch (KindOf(asset))
            {
            case PrefetchKind::Mesh:        (void)ContentLoader::PrefetchMesh(asset, fileSystem); break;
            case PrefetchKind::Texture:     (void)ContentLoader::PrefetchTexture(asset, fileSystem); break;
            case PrefetchKind::Environment: (void)ContentLoader::PrefetchEnvironment(asset, fileSystem); break;
            case PrefetchKind::None:        break;
            }
            ++load.Done;
        }
        load.BackgroundMs = MillisecondsSince(load.Start);
    }

    ECS::SystemPhase SceneRequestSystem::GetPhase() const
    {
        return ECS::SystemPhase::Simulation;
    }

    void SceneRequestSystem::OnFrame(ECS::ECS& /*ecs*/, const ECS::FrameContext& ctx)
    {
        if (ctx.Mode != ECS::PlayMode::Playing)
            return;
        // A frame after a swap, its loads (the meshes then, the renderer's textures and environment
        // since) have taken what they use; the rest is not kept.
        if (m_ClearPrefetched)
        {
            m_ClearPrefetched = false;
            ContentLoader::ClearPrefetched();
        }

        if (m_Async && m_Worker.IsDone(m_Async->Job))
        {
            const std::shared_ptr<AsyncLoad> load = std::exchange(m_Async, nullptr);
            if (!load->Error.empty())
            {
                LOGERROR("[Scene] " + load->Request.Path + " could not be loaded (" + load->Error +
                         "); the running scene is kept.");
                ContentLoader::ClearPrefetched();
                return;
            }
            const auto swapStart = std::chrono::steady_clock::now();
            if (Swap(load->Request, &load->Document))
                LOGINFO("[Scene] Loaded " + load->Request.Path + " in the background in " +
                        std::to_string(std::lround(load->BackgroundMs)) + " ms; the swap took " +
                        std::to_string(std::lround(MillisecondsSince(swapStart))) + " ms.");
            m_ClearPrefetched = true;
            return;
        }

        if (!m_Pending)
            return;
        const PendingScene pending = std::exchange(m_Pending, std::nullopt).value();
        (void)Swap(pending, nullptr);
    }

    bool SceneRequestSystem::Swap(const PendingScene& pending, const YAML::Node* document)
    {
        // The later save load has already run in SaveRequestSystem; if it failed, neither change
        // applies, which its own error explains.
        if (m_Saves.GetLoadRequestCount() != pending.LoadRequestCount)
        {
            LOGWARNING("[Scene] The request to load " + pending.Path
                       + " is dropped: a save load was requested after it.");
            return false;
        }

        const std::optional<std::filesystem::path> physical = m_FileSystem.ResolvePhysical(pending.Path);
        if (!physical)
        {
            LOGERROR("[Scene] " + pending.Path + " cannot be resolved to a file; it is not loaded.");
            return false;
        }

        // Loading clears the world before it reads the document, so one that does not load would
        // leave the game empty: keep the running scene to put back.
        const SceneSnapshot running = m_Scenes.CaptureSnapshot();
        const bool loaded = document ? m_Scenes.LoadSceneFromDocument(*document, pending.Path)
                                     : m_Scenes.LoadScene(physical->string());
        if (!loaded)
        {
            if (m_Scenes.RestoreSnapshot(running))
                LOGERROR("[Scene] " + pending.Path + " could not be loaded; the running scene is kept.");
            else
                LOGERROR("[Scene] " + pending.Path + " could not be loaded, nor the running scene put back.");
            return false;
        }
        m_EventBus.Publish(SceneLoadedEvent{ m_Scenes.GetScenePath() });
        return true;
    }

    void SceneRequestSystem::OnUnregister(ECS::ECS& /*ecs*/)
    {
        Clear();
        m_Worker.Stop();
    }
}
