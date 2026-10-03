#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "HedgehogAudio/api/SoundHandle.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/System.hpp"

namespace FS
{
    class FileSystemManager;
}

namespace HA
{
    class AudioEngine;
}

namespace HedgehogEngine
{
    class CameraSystem;

    // An entity view of the listeners (AudioListenerComponent + TransformComponent).
    class AudioListenerSystem : public ECS::System
    {
    };

    // Plays AudioSourceComponents (with a TransformComponent) through the engine's AudioEngine, in
    // Play mode only: Edit mode plays nothing.
    //
    // OnPlayStart starts every PlayOnStart source with a clip; Pause and Resume hold and carry on
    // its sounds; OnPlayStop stops every sound. Removing a source (RemoveComponent, DestroyEntity)
    // stops its sound. EngineContext::UpdateContext calls Update after Transform and Hierarchy.
    class AudioSystem : public ECS::System
    {
    public:
        // audio and files must outlive the system; files resolves the clips' assets:// paths.
        HEDGEHOG_ENGINE_API AudioSystem(ECS::ECS& ecs, HA::AudioEngine& audio, const FS::FileSystemManager& files);
        HEDGEHOG_ENGINE_API ~AudioSystem() override;

        AudioSystem(const AudioSystem&)            = delete;
        AudioSystem& operator=(const AudioSystem&) = delete;

        void OnPlayStart(ECS::ECS& ecs) override;
        void OnPlayPause(ECS::ECS& ecs) override;
        void OnPlayResume(ECS::ECS& ecs) override;
        void OnPlayStop(ECS::ECS& ecs) override;

        // Starts the entity's source from the beginning (stopping a sound it was playing), at the
        // entity's place, and keeps the sound in its component. An invalid handle when the entity has
        // no source, its clip does not load or the engine is not running.
        HEDGEHOG_ENGINE_API HA::SoundHandle Play(ECS::ECS& ecs, ECS::Entity entity);

        // One frame: the listener's pose (the first active listener by entity id, else the enabled
        // camera with the highest priority, else the default pose at the origin), each playing
        // source's place, volume and pitch from its component, and the release of finished sounds,
        // whose components forget them.
        HEDGEHOG_ENGINE_API void Update(ECS::ECS& ecs, const AudioListenerSystem& listeners, const CameraSystem& cameras);

    private:
        ECS::ECS&                    m_ECS;
        HA::AudioEngine&             m_Audio;
        const FS::FileSystemManager& m_Files;
    };
}
