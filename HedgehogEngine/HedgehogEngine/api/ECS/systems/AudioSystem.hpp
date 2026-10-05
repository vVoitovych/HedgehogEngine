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
    // stops its sound. It runs in the Late phase, after Light, so it reads the world matrices the
    // frame ended with.
    class AudioSystem : public ECS::System
    {
    public:
        // Finds the HA::AudioEngine and FS::FileSystemManager services (the file system resolves
        // the clips' assets:// paths) and makes removing an AudioSourceComponent stop its sound;
        // OnUnregister undoes that. Without the audio engine service the system plays nothing.
        HEDGEHOG_ENGINE_API void OnRegister(ECS::ECS& ecs) override;
        HEDGEHOG_ENGINE_API void OnUnregister(ECS::ECS& ecs) override;

        // Update, with the AudioListenerSystem and CameraSystem it finds in the ECS.
        ECS::SystemPhase         GetPhase() const override { return ECS::SystemPhase::Late; }
        HEDGEHOG_ENGINE_API void OnFrame(ECS::ECS& ecs, const ECS::FrameContext& ctx) override;

        HEDGEHOG_ENGINE_API void OnPlayStart(ECS::ECS& ecs) override;
        HEDGEHOG_ENGINE_API void OnPlayPause(ECS::ECS& ecs) override;
        HEDGEHOG_ENGINE_API void OnPlayResume(ECS::ECS& ecs) override;
        HEDGEHOG_ENGINE_API void OnPlayStop(ECS::ECS& ecs) override;

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
        HA::AudioEngine*             m_Audio = nullptr;
        const FS::FileSystemManager* m_Files = nullptr;
    };
}
