#include "HedgehogEngine/api/ECS/systems/AudioSystem.hpp"

#include "HedgehogEngine/api/ECS/components/AudioListenerComponent.hpp"
#include "HedgehogEngine/api/ECS/components/AudioSourceComponent.hpp"
#include "HedgehogEngine/api/ECS/components/CameraComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/CameraSystem.hpp"
#include "HedgehogEngine/api/Events/SaveEvents.hpp"
#include "HedgehogEngine/api/Events/SceneEvents.hpp"

#include "HedgehogAudio/api/AudioEngine.hpp"
#include "HedgehogAudio/api/AudioPose.hpp"

#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/api/PathUtils.hpp"

#include <algorithm>
#include <optional>
#include <string>

namespace HedgehogEngine
{
    namespace
    {
        // A clip path as written in the inspector: under assets://, prefix optional, either slash.
        std::string ToVirtualPath(std::string path)
        {
            std::replace(path.begin(), path.end(), '\\', '/');
            return FS::ToAssetVirtualPath(path);
        }

        static_assert(HA::AUDIO_BUS_COUNT == 4);
        static_assert(static_cast<int>(AudioSourceBus::Music) == static_cast<int>(HA::AudioBus::Music));
        static_assert(static_cast<int>(AudioSourceBus::Sfx) == static_cast<int>(HA::AudioBus::Sfx));
        static_assert(static_cast<int>(AudioSourceBus::Voice) == static_cast<int>(HA::AudioBus::Voice));
        static_assert(static_cast<int>(AudioSourceBus::Ui) == static_cast<int>(HA::AudioBus::Ui));

        HA::PlayParams MakePlayParams(const AudioSourceComponent& source, const HA::AudioPose& pose)
        {
            HA::PlayParams params;
            params.Volume      = source.Volume;
            params.Pitch       = source.Pitch;
            params.Loop        = source.Loop;
            params.Spatial     = source.Spatial;
            params.Position    = pose.Position;
            params.MinDistance = source.MinDistance;
            params.MaxDistance = source.MaxDistance;
            params.Bus         = static_cast<HA::AudioBus>(source.Bus);
            return params;
        }

        // The first active listener by entity id, else the enabled camera with the highest priority
        // (then the lowest id), else none.
        std::optional<ECS::Entity> FindListener(const ECS::ECS& ecs, const AudioListenerSystem& listeners,
                                                const CameraSystem& cameras)
        {
            std::optional<ECS::Entity> found;
            for (const ECS::Entity entity : listeners.GetEntities())
            {
                if (ecs.GetComponent<AudioListenerComponent>(entity).IsActive && (!found || entity < *found))
                    found = entity;
            }
            if (found)
                return found;

            std::optional<int32_t> bestPriority;
            for (const ECS::Entity entity : cameras.GetEntities())
            {
                const CameraComponent& camera = ecs.GetComponent<CameraComponent>(entity);
                if (!camera.IsEnabled || !ecs.HasComponent<TransformComponent>(entity))
                    continue;
                if (!found || camera.Priority > *bestPriority || (camera.Priority == *bestPriority && entity < *found))
                {
                    found        = entity;
                    bestPriority = camera.Priority;
                }
            }
            return found;
        }
    }

    void AudioSystem::OnRegister(ECS::ECS& ecs)
    {
        m_Audio = ecs.GetServices().Find<HA::AudioEngine>();
        m_Files = ecs.GetServices().Find<FS::FileSystemManager>();
        m_Bus   = ecs.GetServices().Find<EventBus>();
        if (m_Bus)
        {
            // A loaded world's sources have no sound yet; they start in this frame's OnFrame, once its transforms are applied.
            m_SceneSubscription = m_Bus->Subscribe<SceneLoadedEvent>([this](const SceneLoadedEvent&)
            {
                m_StartPending = true;
            });
            m_GameSubscription = m_Bus->Subscribe<GameLoadedEvent>([this](const GameLoadedEvent&)
            {
                m_StartPending = true;
            });
        }
        if (!m_Audio || !ecs.IsComponentRegistered<AudioSourceComponent>())
        {
            return;
        }
        // A source that goes away during Play takes its sound with it.
        ecs.SetComponentRemovedCallback<AudioSourceComponent>([this](ECS::Entity, AudioSourceComponent& source)
        {
            m_Audio->Stop(source.Sound);
            source.Sound = {};
        });
    }

    void AudioSystem::OnUnregister(ECS::ECS& ecs)
    {
        if (m_Bus)
        {
            m_Bus->Unsubscribe(m_SceneSubscription);
            m_Bus->Unsubscribe(m_GameSubscription);
        }
        m_Bus               = nullptr;
        m_SceneSubscription = SubscriptionId::Invalid;
        m_GameSubscription  = SubscriptionId::Invalid;
        if (m_Audio && ecs.IsComponentRegistered<AudioSourceComponent>())
        {
            ecs.SetComponentRemovedCallback<AudioSourceComponent>({});
        }
        m_Audio = nullptr;
        m_Files = nullptr;
    }

    void AudioSystem::OnFrame(ECS::ECS& ecs, const ECS::FrameContext& ctx)
    {
        if (m_StartPending)
        {
            m_StartPending = false;
            if (ctx.Mode == ECS::PlayMode::Playing)
            {
                StartPlayOnStartSources(ecs);
            }
        }
        if (!m_Audio || !ecs.HasSystem<AudioListenerSystem>() || !ecs.HasSystem<CameraSystem>())
        {
            return;
        }
        Update(ecs, *ecs.GetSystem<AudioListenerSystem>(), *ecs.GetSystem<CameraSystem>());
    }

    void AudioSystem::OnPlayStart(ECS::ECS& ecs)
    {
        m_StartPending = false;
        StartPlayOnStartSources(ecs);
    }

    void AudioSystem::StartPlayOnStartSources(ECS::ECS& ecs)
    {
        if (!m_Audio)
        {
            return;
        }
        for (const ECS::Entity entity : GetEntities())
        {
            const AudioSourceComponent& source = ecs.GetComponent<AudioSourceComponent>(entity);
            if (source.PlayOnStart && !source.Clip.empty() && !m_Audio->Exists(source.Sound))
            {
                (void)Play(ecs, entity);
            }
        }
    }

    void AudioSystem::OnPlayPause(ECS::ECS& ecs)
    {
        if (!m_Audio)
        {
            return;
        }
        for (const ECS::Entity entity : GetEntities())
            m_Audio->SetPaused(ecs.GetComponent<AudioSourceComponent>(entity).Sound, true);
    }

    void AudioSystem::OnPlayResume(ECS::ECS& ecs)
    {
        if (!m_Audio)
        {
            return;
        }
        for (const ECS::Entity entity : GetEntities())
            m_Audio->SetPaused(ecs.GetComponent<AudioSourceComponent>(entity).Sound, false);
    }

    void AudioSystem::OnPlayStop(ECS::ECS& ecs)
    {
        m_StartPending = false;
        if (!m_Audio)
        {
            return;
        }
        m_Audio->StopAll();
        for (const ECS::Entity entity : GetEntities())
            ecs.GetComponent<AudioSourceComponent>(entity).Sound = {};
    }

    HA::SoundHandle AudioSystem::Play(ECS::ECS& ecs, ECS::Entity entity)
    {
        if (!m_Audio || !m_Files)
        {
            return {};
        }
        if (!ecs.HasComponent<AudioSourceComponent>(entity) || !ecs.HasComponent<TransformComponent>(entity))
            return {};
        AudioSourceComponent& source = ecs.GetComponent<AudioSourceComponent>(entity);
        m_Audio->Stop(source.Sound);
        source.Sound = {};

        const HA::AudioPose  pose   = HA::MakeAudioPose(ecs.GetComponent<TransformComponent>(entity).ObjMatrix);
        const HA::PlayParams params = MakePlayParams(source, pose);
        if (source.Stream)
        {
            source.Sound = m_Audio->PlayStream(ToVirtualPath(source.Clip), *m_Files, params);
            return source.Sound;
        }
        const HA::AudioClipId clip = m_Audio->LoadClip(ToVirtualPath(source.Clip), *m_Files);
        if (!clip.IsValid())
            return {};
        source.Sound = m_Audio->Play(clip, params);
        return source.Sound;
    }

    void AudioSystem::Update(ECS::ECS& ecs, const AudioListenerSystem& listeners, const CameraSystem& cameras)
    {
        if (!m_Audio)
        {
            return;
        }
        HA::AudioPose listener;
        if (const std::optional<ECS::Entity> entity = FindListener(ecs, listeners, cameras))
            listener = HA::MakeAudioPose(ecs.GetComponent<TransformComponent>(*entity).ObjMatrix);
        m_Audio->SetListenerPose(listener.Position, listener.Forward, listener.Up);

        for (const ECS::Entity entity : GetEntities())
        {
            const AudioSourceComponent& source = ecs.GetComponent<AudioSourceComponent>(entity);
            if (!m_Audio->Exists(source.Sound))
                continue;
            const HA::AudioPose pose = HA::MakeAudioPose(ecs.GetComponent<TransformComponent>(entity).ObjMatrix);
            m_Audio->SetSoundPose(source.Sound, pose.Position, HM::Vector3(0.0f, 0.0f, 0.0f));
            m_Audio->SetVolume(source.Sound, source.Volume);
            m_Audio->SetPitch(source.Sound, source.Pitch);
        }

        m_Audio->Update();
        for (const ECS::Entity entity : GetEntities())
        {
            AudioSourceComponent& source = ecs.GetComponent<AudioSourceComponent>(entity);
            if (source.Sound.IsValid() && !m_Audio->Exists(source.Sound))
                source.Sound = {};
        }
    }
}
