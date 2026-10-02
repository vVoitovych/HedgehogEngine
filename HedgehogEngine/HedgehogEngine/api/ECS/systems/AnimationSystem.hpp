#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"
#include "HedgehogEngine/api/ECS/components/AnimatorComponent.hpp"

#include "HedgehogAnimation/api/AnimationClip.hpp"
#include "HedgehogAnimation/api/Skeleton.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/System.hpp"

#include "HedgehogMath/api/Matrix.hpp"

#include <vector>

namespace HedgehogEngine
{
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
        // empty palette; an unknown clip name shows the bind pose and warns once.
        HEDGEHOG_ENGINE_API void Update(ECS::ECS& ecs, const MeshContainer& meshes, bool playing, float dt);

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
