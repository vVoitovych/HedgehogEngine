#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"
#include "HedgehogEngine/api/Time/FixedStepClock.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "HedgehogInput/api/ActionState.hpp"
#include "HedgehogInput/api/InputActionAsset.hpp"
#include "HedgehogInput/api/InputActionMap.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include <chrono>
#include <memory>
#include <optional>

namespace HedgehogSettings
{
    class Settings;
}

namespace EcsSerialization
{
    class ComponentSerializerRegistry;
}

namespace HA
{
    class AudioEngine;
}

namespace HedgehogEngine
{
    class WindowContext;
    class Camera;

    class TransformSystem;
    class HierarchySystem;
    class MeshSystem;
    class LightSystem;
    class RenderSystem;
    class CameraSystem;
    class AnimationSystem;
    class UiSystem;
    class AudioSystem;
    class AudioListenerSystem;
    class SaveGameManager;
    class PrefabManager;

    // Edit: gameplay does not run. Playing: every frame runs the fixed steps and the update.
    // Paused: nothing runs, and the scene waits to be resumed or stopped.
    enum class PlayState
    {
        Edit,
        Playing,
        Paused
    };

    class EngineContext
    {
    public:
        HEDGEHOG_ENGINE_API EngineContext();
        HEDGEHOG_ENGINE_API ~EngineContext();

        // One frame: the editor camera from the Editor actions (UpdateEditorInput), then gameplay
        // (Play only), animation, transforms, hierarchy and lights. Needs no window.
        HEDGEHOG_ENGINE_API void UpdateContext(float aspectRatio, float dt);

        // Play mode. EngineContext owns the state, the scene snapshot and the clock; the ECS
        // only forwards the events to its systems. Each call returns false and does nothing
        // in the wrong state.
        //   Play   (from Edit):            snapshot the scene, reset the clock, OnPlayStart.
        //   Pause  (from Playing):         OnPlayPause.
        //   Resume (from Paused):          OnPlayResume.
        //   Stop   (from Playing/Paused):  OnPlayStop in reverse order, then restore the snapshot.
        HEDGEHOG_ENGINE_API bool Play();
        HEDGEHOG_ENGINE_API bool Pause();
        HEDGEHOG_ENGINE_API bool Resume();
        HEDGEHOG_ENGINE_API bool Stop();

        [[nodiscard]] HEDGEHOG_ENGINE_API PlayState GetPlayState() const;
        [[nodiscard]] HEDGEHOG_ENGINE_API FixedStepClock&       GetFixedStepClock();
        [[nodiscard]] HEDGEHOG_ENGINE_API const FixedStepClock& GetFixedStepClock() const;

        // One frame of gameplay, only while Playing: the clock's fixed steps of OnFixedUpdate,
        // then one OnUpdate with the scaled frame time, then the save and load requests the frame
        // made (SaveGameManager::ProcessRequests). UpdateContext calls it; it is public so tests
        // can drive frames without a window.
        HEDGEHOG_ENGINE_API void UpdatePlayMode(float dt);

        // Runs the Animation phase (AnimationSystem), which fills every animator's skinning palette
        // for this frame: advanced by the frame's scaled time while Playing, held while Paused, the
        // bind pose (or a preview time) in Edit. UpdateContext calls it right after UpdatePlayMode;
        // public for the same reason.
        HEDGEHOG_ENGINE_API void UpdateAnimation(float dt);

        // Input actions, from assets://Input/actions.yaml (INPUT_ACTIONS_PATH; the defaults when it
        // is missing or does not parse). UpdateGameInput evaluates the Game map from the input the
        // application hands it while Playing, and resets the state in Edit and Paused, so nothing
        // there sees an action; the application calls it each frame before UpdateContext, tests
        // call it directly. A system that handles an action consumes it through GetGameActionState,
        // right after UpdateGameInput and before UpdatePlayMode. The game UI does so inside it while
        // Playing (UiSystem::UpdateInput, over a game view of gameViewSize pixels, the size the UI is
        // extracted at; a zero size gives the UI no input), and its input is reset in Edit and Paused.
        static constexpr const char* INPUT_ACTIONS_PATH = "assets://Input/actions.yaml";
        HEDGEHOG_ENGINE_API void UpdateGameInput(const HW::RawInput& gameInput,
                                                 const HM::Vector2& gameViewSize = HM::Vector2(0.0f, 0.0f));

        // Evaluates the Editor map (the editor camera's actions) from the input the application hands
        // it, in every play state: the Editor calls it each frame with the scene panel's input before
        // UpdateContext, which moves the camera from it. --game-mode never calls it, so its camera
        // stays still.
        HEDGEHOG_ENGINE_API void UpdateEditorInput(const HW::RawInput& editorInput);

        // Re-reads the actions file once it has been saved (polled at most once a second; now is a
        // parameter so tests need not wait), resetting the game action state. UpdateContext calls it.
        HEDGEHOG_ENGINE_API void ReloadInputActions(std::chrono::steady_clock::time_point now);

        [[nodiscard]] HEDGEHOG_ENGINE_API const HInput::InputActionSet& GetInputActions() const;
        [[nodiscard]] HEDGEHOG_ENGINE_API HInput::ActionState&          GetGameActionState();
        [[nodiscard]] HEDGEHOG_ENGINE_API const HInput::ActionState&    GetGameActionState() const;

        HEDGEHOG_ENGINE_API ResourceCatalog&       GetResourceCatalog();
        HEDGEHOG_ENGINE_API const ResourceCatalog& GetResourceCatalog() const;

        HEDGEHOG_ENGINE_API SceneManager&       GetSceneManager();
        HEDGEHOG_ENGINE_API const SceneManager& GetSceneManager() const;

        // Save games in Play mode. Stop drops its pending requests; the application sets its save
        // directory (the Editor's per-project editor folder, --game-mode's game folder).
        HEDGEHOG_ENGINE_API SaveGameManager& GetSaveGames();

        // Prefab assets: create one from a subtree, instantiate one into the scene.
        HEDGEHOG_ENGINE_API PrefabManager& GetPrefabs();

        // How every component is written to and read from scenes, snapshots, saves and prefabs.
        [[nodiscard]] HEDGEHOG_ENGINE_API const EcsSerialization::ComponentSerializerRegistry& GetComponentRegistry() const;

        HEDGEHOG_ENGINE_API HedgehogSettings::Settings&       GetSettings();
        HEDGEHOG_ENGINE_API const HedgehogSettings::Settings& GetSettings() const;

        HEDGEHOG_ENGINE_API const Camera& GetCamera() const;

        HEDGEHOG_ENGINE_API EventBus&            GetEventBus();

        HEDGEHOG_ENGINE_API ECS::ECS&           GetECS();
        HEDGEHOG_ENGINE_API TransformSystem*    GetTransformSystem() const;
        HEDGEHOG_ENGINE_API HierarchySystem*    GetHierarchySystem() const;
        HEDGEHOG_ENGINE_API MeshSystem*         GetMeshSystem()      const;
        HEDGEHOG_ENGINE_API LightSystem*        GetLightSystem()     const;
        HEDGEHOG_ENGINE_API RenderSystem*       GetRenderSystem()    const;
        HEDGEHOG_ENGINE_API CameraSystem*       GetCameraSystem()    const;
        HEDGEHOG_ENGINE_API AnimationSystem*    GetAnimationSystem() const;
        HEDGEHOG_ENGINE_API UiSystem*           GetUiSystem()        const;
        HEDGEHOG_ENGINE_API AudioSystem*        GetAudioSystem()     const;

        // The audio engine AudioSystem plays through. The context never starts it, so tests and
        // tools stay silent: the application calls Init (the Editor and --game-mode with a device,
        // tests with NoDevice). Until then every sound is skipped.
        HEDGEHOG_ENGINE_API HA::AudioEngine& GetAudioEngine();

        HEDGEHOG_ENGINE_API const FS::FileSystemManager& GetFileSystem() const;
        // Mutable, so an application can mount its own folders (a game's data) before loading.
        HEDGEHOG_ENGINE_API FS::FileSystemManager& GetFileSystem();

    private:
        void RegisterServices();
        // What this frame's phases get: dt, its scaled form, the fixed step and the play mode.
        [[nodiscard]] ECS::FrameContext MakeFrameContext(float dt) const;
        void InitECS();
        void InitFileSystem();
        void RegisterComponents();
        void LoadInputActions();
        void UpdateCamera(float aspectRatio, float dt);

    private:
        FS::FileSystemManager m_FileSystem;

        EventBus m_EventBus;

        std::unique_ptr<Camera> m_Camera;

        // Before the ECS, so it outlives AudioSystem and the sounds the components hold.
        std::unique_ptr<HA::AudioEngine> m_AudioEngine;

        ECS::ECS m_ECS;

        std::shared_ptr<TransformSystem>  m_TransformSystem;
        std::shared_ptr<HierarchySystem>  m_HierarchySystem;
        std::shared_ptr<MeshSystem>       m_MeshSystem;
        std::shared_ptr<LightSystem>      m_LightSystem;
        std::shared_ptr<RenderSystem>     m_RenderSystem;
        std::shared_ptr<CameraSystem>     m_CameraSystem;
        std::shared_ptr<AnimationSystem>  m_AnimationSystem;
        std::shared_ptr<UiSystem>         m_UiSystem;
        std::shared_ptr<AudioSystem>         m_AudioSystem;
        std::shared_ptr<AudioListenerSystem> m_AudioListenerSystem;

        ResourceCatalog m_ResourceCatalog;

        std::unique_ptr<HedgehogSettings::Settings>                    m_Settings;
        std::unique_ptr<EcsSerialization::ComponentSerializerRegistry> m_ComponentRegistry;

        // Constructed after ECS/systems/component-registry are ready (it creates the scene root
        // and needs live system references) — see EngineContext.cpp for the ordering.
        std::unique_ptr<SceneManager> m_SceneManager;
        std::unique_ptr<SaveGameManager> m_SaveGames; // after the SceneManager it saves and the Settings it reads
        std::unique_ptr<PrefabManager>   m_Prefabs;   // after the SceneManager it refreshes

        HInput::InputActionSet    m_InputActions;
        HInput::InputActionsWatch m_InputWatch;
        HInput::ActionState       m_GameActions;
        HInput::ActionState       m_EditorActions;

        PlayState                    m_PlayState = PlayState::Edit;
        FixedStepClock               m_Clock;
        std::optional<SceneSnapshot> m_PlaySnapshot;
    };
}
