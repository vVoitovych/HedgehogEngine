#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"
#include "HedgehogEngine/api/ECS/components/AnimatorComponent.hpp"

#include "HedgehogAnimation/api/AnimationClip.hpp"
#include "HedgehogAnimation/api/AnimatorController.hpp"
#include "HedgehogAnimation/api/Skeleton.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/System.hpp"

#include "HedgehogMath/api/Matrix.hpp"

#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace FS
{
    class FileSystemManager;
}

namespace HedgehogEngine
{
    class EventBus;
    class MeshContainer;
    class ResourceCatalog;

    // Fills each animated entity's skinning palette (AnimatorComponent + MeshComponent).
    //
    // It runs in the Animation phase, which comes after Simulation, where the play-mode fixed
    // steps and update (the scripts) run: a clip a script picks in its OnUpdate shows in the same
    // frame, whatever order the systems were registered in. It overrides no play-mode event.
    class AnimationSystem : public ECS::System
    {
    public:
        // Finds the EventBus and ResourceCatalog services it updates with (without both, OnFrame
        // does nothing) and the FileSystemManager it reads controllers with (without it, none).
        HEDGEHOG_ENGINE_API void OnRegister(ECS::ECS& ecs) override;
        HEDGEHOG_ENGINE_API void OnUnregister(ECS::ECS& ecs) override;

        // Update with the catalog's meshes: playing outside Edit, advancing by the frame's scaled
        // time while Playing and not at all while Paused.
        ECS::SystemPhase         GetPhase() const override { return ECS::SystemPhase::Animation; }
        HEDGEHOG_ENGINE_API void OnFrame(ECS::ECS& ecs, const ECS::FrameContext& ctx) override;

        // One frame. In Play (playing, dt the frame's scaled time; 0 while paused) each animator
        // starts on its first frame (playing Clip when PlayOnStart), switches when Clip changes
        // (crossfading over CrossfadeTime from where the old clips were: a switch during a fade
        // keeps the unfinished blend fading out, through the animator's BlendStack), advances by
        // dt * Speed and fills Palette. The current clip loops as Loop says unless Play gave it a
        // loop of its own; a clip fading out keeps the loop it played with. In Edit (not playing) it shows Clip at PreviewTime when set, else the
        // bind pose, and forgets its play state. An entity whose mesh has no skeleton gets an
        // empty palette; an unknown clip name shows the bind pose and warns once. A non-looping
        // clip that reaches its end publishes AnimationFinishedEvent on bus once.
        //
        // An animator whose Controller reads, validates and finds every state's clip on the mesh
        // runs it instead of Clip: Play starts its default state, each frame advances the state's
        // clip and then steps the machine by dt * Speed, and a transition that fires enters its
        // state from the clip's start, crossfading over the transition's duration; each state
        // change publishes AnimatorStateChangedEvent. A controller that does not read or validate
        // logs its problems once per version of the file, and a state's clip the mesh lacks once
        // per animator; then Clip plays.
        HEDGEHOG_ENGINE_API void Update(ECS::ECS& ecs, const MeshContainer& meshes, EventBus& bus, bool playing,
                                        float dt);

        // Plays clip on entity's animator from its start, crossfading out of what plays over fade
        // seconds (CrossfadeTime when not given), looping as loop says (Loop when not given).
        // Playing the current clip again restarts it without a fade. A clip the entity's mesh
        // does not have logs a warning naming it and keeps the current clip; returns whether the
        // clip was found. With a controller clip names a state, entered from its start at the
        // next frame (crossfading as above; loop is the state's), false with a warning when the
        // controller has no such state.
        HEDGEHOG_ENGINE_API bool Play(ECS::ECS& ecs, const MeshContainer& meshes, ECS::Entity entity,
                                      const std::string& clip, std::optional<float> fade = std::nullopt,
                                      std::optional<bool> loop = std::nullopt);

        // Seconds into the animator's current clip; 0 when nothing plays.
        [[nodiscard]] HEDGEHOG_ENGINE_API static float GetTime(const AnimatorComponent& animator);
        // Moves the current clip to time seconds and lets a finished clip finish again.
        HEDGEHOG_ENGINE_API static void SetTime(AnimatorComponent& animator, float time);

        // Stops entity's animator: no clip plays and the mesh shows its bind pose. A controller's
        // machine keeps its state and parameters, and waits for Play to name a state.
        HEDGEHOG_ENGINE_API void Stop(ECS::ECS& ecs, ECS::Entity entity);

        // The names of the clips of entity's mesh, in file order; empty without a skinned mesh.
        HEDGEHOG_ENGINE_API std::vector<std::string> GetClipNames(const ECS::ECS& ecs, const MeshContainer& meshes,
                                                                  ECS::Entity entity) const;

        // How often ReloadChangedControllers looks at the files.
        static constexpr std::chrono::milliseconds CONTROLLER_POLL_INTERVAL{ 1000 };

        // Reads again every controller file whose write time has moved on, at most once per
        // CONTROLLER_POLL_INTERVAL (now is a parameter, so tests need not wait), logging
        // "[Animation] Reloaded <path>."; OnFrame calls it. Its animators start its default state
        // again with the parameters' defaults.
        HEDGEHOG_ENGINE_API void ReloadChangedControllers(std::chrono::steady_clock::time_point now);

        // entity's controller, read on first use and cached by its file (however the path is
        // spelt); nullptr when it names none, or one that does not read or validate.
        HEDGEHOG_ENGINE_API const HedgehogAnimation::AnimatorController* GetController(ECS::ECS& ecs, ECS::Entity entity);

        // Sets a parameter of entity's controller, before Play too: false, changing nothing, for
        // no controller, an unknown name or another type. A trigger stays set until a transition
        // testing it fires.
        HEDGEHOG_ENGINE_API bool SetFloat(ECS::ECS& ecs, ECS::Entity entity, const std::string& name, float value);
        HEDGEHOG_ENGINE_API bool SetInt(ECS::ECS& ecs, ECS::Entity entity, const std::string& name, int value);
        HEDGEHOG_ENGINE_API bool SetBool(ECS::ECS& ecs, ECS::Entity entity, const std::string& name, bool value);
        HEDGEHOG_ENGINE_API bool SetTrigger(ECS::ECS& ecs, ECS::Entity entity, const std::string& name, bool set = true);
        // The parameter's value (1 or 0 for a Bool or Trigger); nullopt without one.
        HEDGEHOG_ENGINE_API std::optional<float> GetParameter(ECS::ECS& ecs, ECS::Entity entity, const std::string& name);
        // The state entity's controller plays; empty when no controller runs.
        HEDGEHOG_ENGINE_API std::string GetStateName(ECS::ECS& ecs, ECS::Entity entity);

    private:
        void Evaluate(AnimatorComponent& animator, const HedgehogAnimation::Skeleton& skeleton,
                      const std::vector<HedgehogAnimation::AnimationClip>& clips, ECS::Entity entity);
        // The clips are the mesh's (none when it is not loaded yet); meshIndex names it.
        void Advance(AnimatorComponent& animator, const std::vector<HedgehogAnimation::AnimationClip>* clips,
                     std::optional<uint64_t> meshIndex, float dt, ECS::Entity entity, EventBus* bus);

        // A controller file, read once for every animator naming it.
        struct ControllerSlot
        {
            std::string                                      Key; // FS::MakeAssetKey of its path
            std::filesystem::file_time_type                  WriteTime{};
            uint32_t                                         Version = 0;
            std::optional<HedgehogAnimation::AnimatorController> Controller; // empty when it does not read or validate
        };
        using ParameterSetter =
            std::function<bool(const HedgehogAnimation::AnimatorController&, HedgehogAnimation::StateMachineInstance&)>;

        void LoadController(ControllerSlot& slot);
        // The animator's controller (its slot found or read when Controller changed), or nullptr.
        const HedgehogAnimation::AnimatorController* FindController(AnimatorComponent& animator);
        // Each state's clip length on the mesh, worked out once per mesh; whether every clip is there.
        bool PrepareController(AnimatorComponent& animator, const HedgehogAnimation::AnimatorController& controller,
                               const std::vector<HedgehogAnimation::AnimationClip>& clips, std::optional<uint64_t> meshIndex,
                               ECS::Entity entity);
        void AdvanceController(AnimatorComponent& animator, const HedgehogAnimation::AnimatorController& controller,
                               const std::vector<HedgehogAnimation::AnimationClip>& clips, std::optional<uint64_t> meshIndex,
                               float dt, ECS::Entity entity, EventBus* bus);
        void EnterState(AnimatorComponent& animator, const HedgehogAnimation::AnimatorController& controller,
                        const std::vector<HedgehogAnimation::AnimationClip>* clips, uint32_t state, float fade,
                        ECS::Entity entity, EventBus* bus, const std::string& from);
        bool SetParameter(ECS::ECS& ecs, ECS::Entity entity, const ParameterSetter& set);

    private:
        EventBus*              m_Bus     = nullptr;
        ResourceCatalog*       m_Catalog = nullptr;
        FS::FileSystemManager* m_Files   = nullptr;

        std::vector<ControllerSlot>           m_Controllers;
        std::chrono::steady_clock::time_point m_LastControllerPoll{};

        // Scratch poses reused every frame, so steady-state evaluation allocates nothing.
        std::vector<HedgehogAnimation::JointTransform> m_Pose;
        std::vector<HedgehogAnimation::JointTransform> m_Scratch;
        std::vector<HM::Matrix4x4>                     m_ModelPose;
    };
}
