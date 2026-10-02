#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"
#include "HedgehogEngine/api/ECS/components/AnimatorComponent.hpp"

#include "HedgehogAnimation/api/AnimationClip.hpp"
#include "HedgehogAnimation/api/Skeleton.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/System.hpp"

#include "HedgehogMath/api/Matrix.hpp"

#include <optional>
#include <string>
#include <vector>

namespace HedgehogEngine
{
    class EventBus;
    class MeshContainer;

    // Fills each animated entity's skinning palette (AnimatorComponent + MeshComponent).
    //
    // It overrides no play-mode event: play-mode events run in registration order, and the engine
    // registers its systems before the application registers ScriptSystem, so an OnUpdate here
    // would run before the scripts. EngineContext::UpdateContext calls Update right after
    // UpdatePlayMode instead, so a clip a script picks in its OnUpdate shows in the same frame.
    class AnimationSystem : public ECS::System
    {
    public:
        // One frame. In Play (playing, dt the frame's scaled time; 0 while paused) each animator
        // starts on its first frame (playing Clip when PlayOnStart), switches when Clip changes
        // (crossfading over CrossfadeTime from where the old clip was), advances by dt * Speed and
        // fills Palette. In Edit (not playing) it shows Clip at PreviewTime when set, else the
        // bind pose, and forgets its play state. An entity whose mesh has no skeleton gets an
        // empty palette; an unknown clip name shows the bind pose and warns once. A non-looping
        // clip that reaches its end publishes AnimationFinishedEvent on bus once.
        HEDGEHOG_ENGINE_API void Update(ECS::ECS& ecs, const MeshContainer& meshes, EventBus& bus, bool playing,
                                        float dt);

        // Plays clip on entity's animator from its start, crossfading out of the current one over
        // fade seconds (CrossfadeTime when not given). Playing the current clip again restarts it
        // without a fade. A clip the entity's mesh does not have logs a warning naming it and
        // keeps the current clip; returns whether the clip was found.
        HEDGEHOG_ENGINE_API bool Play(ECS::ECS& ecs, const MeshContainer& meshes, ECS::Entity entity,
                                      const std::string& clip, std::optional<float> fade = std::nullopt);

        // Stops entity's animator: no clip plays and the mesh shows its bind pose.
        HEDGEHOG_ENGINE_API void Stop(ECS::ECS& ecs, ECS::Entity entity);

        // The names of the clips of entity's mesh, in file order; empty without a skinned mesh.
        HEDGEHOG_ENGINE_API std::vector<std::string> GetClipNames(const ECS::ECS& ecs, const MeshContainer& meshes,
                                                                  ECS::Entity entity) const;

    private:
        void Evaluate(AnimatorComponent& animator, const HedgehogAnimation::Skeleton& skeleton,
                      const std::vector<HedgehogAnimation::AnimationClip>& clips, ECS::Entity entity);
        void Advance(AnimatorComponent& animator, float dt);

    private:
        // Scratch poses reused every frame, so steady-state evaluation allocates nothing.
        std::vector<HedgehogAnimation::JointTransform> m_Pose;
        std::vector<HedgehogAnimation::JointTransform> m_FadePose;
        std::vector<HedgehogAnimation::JointTransform> m_Blended;
        std::vector<HM::Matrix4x4>                     m_ModelPose;
    };
}
