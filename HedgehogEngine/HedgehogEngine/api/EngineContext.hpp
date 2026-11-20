#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"
#include "HedgehogEngine/api/App/ApplicationRequests.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Input/GameInputFrame.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"
#include "HedgehogEngine/api/Time/FixedStepClock.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "HedgehogEngine/HedgehogSettings/api/PlayerSettings.hpp"
#include "HedgehogEngine/HedgehogWindow/api/RawInput.hpp"

#include "HedgehogInput/api/ActionState.hpp"
#include "HedgehogInput/api/InputActionAsset.hpp"
#include "HedgehogInput/api/InputActionMap.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace HedgehogSettings
{
    class Settings;
}

namespace EcsSerialization
{
    class ComponentSerializerRegistry;
    class ComponentTypeRegistry;
}

namespace HA
{
    class AudioEngine;
}

namespace HP
{
    class PhysicsWorld;
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
    class PhysicsSystem;
    class RigidBodyListSystem;
    class EnvironmentSystem;
    class TerrainSystem;
    class ParticleSystem;
    class SaveGameManager;
    class SceneRequestSystem;
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

        // One frame: the editor camera from the Editor actions (UpdateEditorInput), then one
        // ECS::RunPhases from Simulation to Sync: gameplay and save requests (Play only),
        // animation, transforms and hierarchy, lights and audio, then the meshes and materials
        // listed this frame loaded into the resource catalog. Needs no window.
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

        // Switches to another scene at the end of the frame (SceneRequestSystem, after the frame's
        // saves), publishing SceneLoadedEvent. Playing or Paused only, path a .yaml that exists
        // under assets:// (or another mount) without `..`; false, logged, otherwise. The last
        // request of a frame wins, and so does the later of this and a save load (the other is
        // dropped with a warning). Stop drops a pending request and restores the scene and path
        // from before Play. The fixed-step clock is not reset.
        // With async the scene is read on a background thread first, the current scene playing on
        // meanwhile: its YAML parsed and every mesh, texture and environment it reaches decoded
        // ahead (ContentLoader's prefetch store), then swapped in at the end of the first frame after
        // (SceneLoadedEvent as above). A later request, a sync one included, cancels it; so does Stop.
        [[nodiscard]] HEDGEHOG_ENGINE_API bool RequestSceneLoad(const std::string& path, bool async = false);
        // An asynchronous scene load's progress, 0 to 1 (its assets decoded so far); nullopt with none.
        [[nodiscard]] HEDGEHOG_ENGINE_API std::optional<float> GetSceneLoadProgress() const;

        [[nodiscard]] HEDGEHOG_ENGINE_API PlayState GetPlayState() const;
        [[nodiscard]] HEDGEHOG_ENGINE_API FixedStepClock&       GetFixedStepClock();
        [[nodiscard]] HEDGEHOG_ENGINE_API const FixedStepClock& GetFixedStepClock() const;

        // Runs the Simulation phase alone. While Playing it advances the clock and runs its fixed
        // steps of OnFixedUpdate, then one OnUpdate with the scaled frame time, then the save and
        // load requests the frame made (SaveRequestSystem, SaveGameManager::ProcessRequests) and
        // the scene request (SceneRequestSystem);
        // outside Play it does nothing. UpdateContext runs the same phase itself (calling both
        // would advance the clock twice); this is public so tests can drive gameplay without a
        // window.
        HEDGEHOG_ENGINE_API void UpdatePlayMode(float dt);

        // Runs the Animation phase (AnimationSystem), which fills every animator's skinning palette
        // for this frame: advanced by the frame's scaled time while Playing, held while Paused, the
        // bind pose (or a preview time) in Edit. UpdateContext runs this phase right after Simulation;
        // public for the same reason.
        HEDGEHOG_ENGINE_API void UpdateAnimation(float dt);

        // Input actions, from assets://Input/actions.yaml (INPUT_ACTIONS_PATH; the defaults when it
        // is missing or does not parse). UpdateGameInput evaluates the Game map from the input the
        // application hands it while Playing, and resets the state in Edit and Paused, so nothing
        // there sees an action; the application calls it each frame before UpdateContext, tests
        // call it directly. It then writes the GameInputFrame service (the map, the action state and
        // gameViewSize, the size the UI is extracted at) and runs the ECS's Input phase, whose
        // systems handle actions and consume them, so gameplay in the Simulation phase no longer
        // sees them. The game UI is one (UiSystem: UpdateInput while Playing, a zero size giving it
        // no input; its input reset in Edit and Paused).
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

        // The effective actions: the project's file with the player's overrides applied to its Game map
        // (HInput::ApplyInputOverrides; the Editor map is never overridden). Changing the overrides, or
        // the project's file reloading, merges them again and resets both action states.
        [[nodiscard]] HEDGEHOG_ENGINE_API const HInput::InputActionSet& GetInputActions() const;
        [[nodiscard]] HEDGEHOG_ENGINE_API const HInput::InputActionSet& GetProjectInputActions() const;

        // The player's rebinding of Game actions, kept in INPUT_OVERRIDES_PATH in the actions file
        // format (its Game section). LoadInputOverrides: false for a missing file (nothing changes) and,
        // with one [Input] error, for one that does not parse (the overrides cleared, so the project's
        // bindings play). SaveInputOverrides: false, logged, when it cannot be written.
        static constexpr const char* INPUT_OVERRIDES_PATH = "player://input_overrides.yaml";
        HEDGEHOG_ENGINE_API void SetInputOverrides(HInput::InputActionMap overrides);
        [[nodiscard]] HEDGEHOG_ENGINE_API const HInput::InputActionMap& GetInputOverrides() const;
        HEDGEHOG_ENGINE_API bool LoadInputOverrides(const std::string& virtualPath = INPUT_OVERRIDES_PATH);
        [[nodiscard]] HEDGEHOG_ENGINE_API bool SaveInputOverrides(
            const std::string& virtualPath = INPUT_OVERRIDES_PATH) const;

        // The game's input as UpdateGameInput last had it, in every play state (for listening for a
        // press to bind).
        [[nodiscard]] HEDGEHOG_ENGINE_API const HW::RawInput& GetLastGameInput() const;

        [[nodiscard]] HEDGEHOG_ENGINE_API HInput::ActionState&          GetGameActionState();
        [[nodiscard]] HEDGEHOG_ENGINE_API const HInput::ActionState&    GetGameActionState() const;

        [[nodiscard]] HEDGEHOG_ENGINE_API ApplicationRequests&       GetApplicationRequests();
        [[nodiscard]] HEDGEHOG_ENGINE_API const ApplicationRequests& GetApplicationRequests() const;

        // What the player chose, an ECS service. The engine only keeps and flags it: each application
        // decides what it applies, and compares the version with the one it last applied.
        [[nodiscard]] HEDGEHOG_ENGINE_API HedgehogSettings::PlayerSettings&       GetPlayerSettings();
        [[nodiscard]] HEDGEHOG_ENGINE_API const HedgehogSettings::PlayerSettings& GetPlayerSettings() const;
        HEDGEHOG_ENGINE_API void                                                  MarkPlayerSettingsChanged();
        [[nodiscard]] HEDGEHOG_ENGINE_API uint64_t                                GetPlayerSettingsVersion() const;
        // The player's master and bus volumes onto the audio engine.
        HEDGEHOG_ENGINE_API void ApplyPlayerVolumes();

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

        // Every component type: the engine's 19 in serializer order, then any an application
        // adds. Also the ECS's ComponentTypeRegistry service. Menus and the inspector list it.
        [[nodiscard]] HEDGEHOG_ENGINE_API EcsSerialization::ComponentTypeRegistry&       GetComponentTypes();
        [[nodiscard]] HEDGEHOG_ENGINE_API const EcsSerialization::ComponentTypeRegistry& GetComponentTypes() const;

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
        HEDGEHOG_ENGINE_API PhysicsSystem*      GetPhysicsSystem()   const;
        // The entity view of the environments, which extraction reads.
        HEDGEHOG_ENGINE_API EnvironmentSystem*  GetEnvironmentSystem() const;
        // The terrains, which resolve their files in the Sync phase.
        HEDGEHOG_ENGINE_API TerrainSystem*      GetTerrainSystem() const;
        HEDGEHOG_ENGINE_API ParticleSystem*     GetParticleSystem() const;

        // The audio engine AudioSystem plays through. The context never starts it, so tests and
        // tools stay silent: the application calls Init (the Editor and --game-mode with a device,
        // tests with NoDevice). Until then every sound is skipped.
        HEDGEHOG_ENGINE_API HA::AudioEngine& GetAudioEngine();

        // The physics world PhysicsSystem simulates in. It starts at the first Play with a collider,
        // from the project's physics settings, so a context without colliders never starts Jolt.
        HEDGEHOG_ENGINE_API HP::PhysicsWorld& GetPhysicsWorld();

        HEDGEHOG_ENGINE_API const FS::FileSystemManager& GetFileSystem() const;
        // Mutable, so an application can mount its own folders (a game's data) before loading.
        HEDGEHOG_ENGINE_API FS::FileSystemManager& GetFileSystem();

    private:
        void RegisterServices();
        // What this frame's phases get: dt, its scaled form, the fixed step and the play mode.
        [[nodiscard]] ECS::FrameContext MakeFrameContext(float dt) const;
        // MakeFrameContext, with the clock advanced (Playing only) and its steps as FixedSteps.
        // Called once per frame, by whichever entry runs the Simulation phase.
        [[nodiscard]] ECS::FrameContext BeginFrame(float dt);
        void InitECS();
        void InitFileSystem();
        void LoadInputActions();
        void MergeInputActions(); // m_InputActions from the project's and the overrides
        void UpdateCamera(float aspectRatio, float dt);

    private:
        FS::FileSystemManager m_FileSystem;

        EventBus m_EventBus;

        std::unique_ptr<Camera> m_Camera;

        // Before the ECS, so it outlives AudioSystem and the sounds the components hold.
        std::unique_ptr<HA::AudioEngine> m_AudioEngine;
        // Likewise, so it outlives PhysicsSystem and the bodies it made.
        std::unique_ptr<HP::PhysicsWorld> m_PhysicsWorld;

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
        std::shared_ptr<PhysicsSystem>       m_PhysicsSystem;
        std::shared_ptr<RigidBodyListSystem> m_RigidBodyListSystem;
        std::shared_ptr<EnvironmentSystem>   m_EnvironmentSystem;
        std::shared_ptr<TerrainSystem>       m_TerrainSystem;
        std::shared_ptr<ParticleSystem>      m_ParticleSystem;

        ResourceCatalog m_ResourceCatalog;

        std::unique_ptr<HedgehogSettings::Settings>                    m_Settings;
        std::unique_ptr<EcsSerialization::ComponentSerializerRegistry> m_ComponentRegistry;
        std::unique_ptr<EcsSerialization::ComponentTypeRegistry>       m_ComponentTypes; // writes into the one above

        // Constructed after ECS/systems/component-registry are ready (it creates the scene root
        // and needs live system references) — see EngineContext.cpp for the ordering.
        std::unique_ptr<SceneManager> m_SceneManager;
        std::unique_ptr<SaveGameManager> m_SaveGames; // after the SceneManager it saves and the Settings it reads
        std::shared_ptr<SceneRequestSystem> m_SceneRequests;
        std::unique_ptr<PrefabManager>   m_Prefabs;   // after the SceneManager it refreshes

        HInput::InputActionSet    m_ProjectInputActions; // assets://Input/actions.yaml
        HInput::InputActionMap    m_InputOverrides;
        HInput::InputActionSet    m_InputActions; // the two merged; GameInputFrame points into it
        HW::RawInput              m_LastGameInput;
        HInput::InputActionsWatch m_InputWatch;
        HInput::ActionState       m_GameActions;
        HInput::ActionState       m_EditorActions;
        GameInputFrame            m_GameInput; // a service: the Input phase's view of the above
        ApplicationRequests       m_ApplicationRequests; // a service: what scripts ask of the host
        HedgehogSettings::PlayerSettings m_PlayerSettings; // a service
        uint64_t                         m_PlayerSettingsVersion = 0;

        PlayState                    m_PlayState = PlayState::Edit;
        FixedStepClock               m_Clock;
        std::optional<SceneSnapshot> m_PlaySnapshot;
    };
}
