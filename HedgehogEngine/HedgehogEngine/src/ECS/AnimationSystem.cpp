#include "HedgehogEngine/api/ECS/systems/AnimationSystem.hpp"

#include "HedgehogEngine/api/Containers/Mesh.hpp"
#include "HedgehogEngine/api/Containers/MeshContainer.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"

#include "HedgehogAnimation/api/Pose.hpp"

#include "Logger/api/Logger.hpp"

namespace HedgehogEngine
{
    namespace
    {
        const HedgehogAnimation::AnimationClip* FindClip(const std::vector<HedgehogAnimation::AnimationClip>& clips,
                                                         const std::string& name)
        {
            for (const HedgehogAnimation::AnimationClip& clip : clips)
                if (clip.Name == name)
                    return &clip;
            return nullptr;
        }

        // Back to how a freshly loaded animator starts.
        void ResetPlayState(AnimatorComponent& animator)
        {
            animator.Started      = false;
            animator.Playing      = false;
            animator.CurrentClip.clear();
            animator.Time         = 0.0f;
            animator.PreviousClip.clear();
            animator.PreviousTime = 0.0f;
            animator.FadeElapsed  = 0.0f;
        }
    }

    void AnimationSystem::Update(ECS::ECS& ecs, const MeshContainer& meshes, bool playing, float dt)
    {
        for (const ECS::Entity entity : m_Entities)
        {
            AnimatorComponent&   animator = ecs.GetComponent<AnimatorComponent>(entity);
            const MeshComponent& mesh     = ecs.GetComponent<MeshComponent>(entity);

            if (playing)
                Advance(animator, dt);
            else if (animator.Started)
                ResetPlayState(animator);

            const HedgehogAnimation::Skeleton* skeleton = nullptr;
            const Mesh*                        loaded   = nullptr;
            if (mesh.MeshIndex && *mesh.MeshIndex < meshes.GetMeshCount())
            {
                loaded   = &meshes.GetMesh(static_cast<size_t>(*mesh.MeshIndex));
                skeleton = loaded->GetSkeleton();
            }
            if (!skeleton)
            {
                animator.Palette.clear();
                continue;
            }
            Evaluate(animator, *skeleton, loaded->GetAnimationClips(), entity);
        }
    }

    void AnimationSystem::Advance(AnimatorComponent& animator, float dt)
    {
        if (!animator.Started)
        {
            animator.Started     = true;
            animator.Playing     = animator.PlayOnStart && !animator.Clip.empty();
            animator.CurrentClip = animator.Playing ? animator.Clip : std::string{};
            animator.Time        = 0.0f;
            return; // the first frame shows time 0
        }

        if (animator.Clip != animator.CurrentClip)
        {
            // A new clip plays from its start; the old one fades out from where it was, unless
            // nothing was playing or there is no fade time.
            const bool fade = animator.Playing && !animator.CurrentClip.empty() && animator.CrossfadeTime > 0.0f;
            animator.PreviousClip = fade ? animator.CurrentClip : std::string{};
            animator.PreviousTime = animator.Time;
            animator.FadeElapsed  = 0.0f;
            animator.CurrentClip  = animator.Clip;
            animator.Time         = 0.0f;
            animator.Playing      = !animator.Clip.empty();
            return; // the new clip shows at time 0 this frame
        }

        if (!animator.Playing)
            return;
        animator.Time += dt * animator.Speed;
        if (!animator.PreviousClip.empty())
        {
            animator.PreviousTime += dt * animator.Speed;
            animator.FadeElapsed  += dt;
            if (animator.FadeElapsed >= animator.CrossfadeTime)
                animator.PreviousClip.clear();
        }
    }

    void AnimationSystem::Evaluate(AnimatorComponent& animator, const HedgehogAnimation::Skeleton& skeleton,
                                   const std::vector<HedgehogAnimation::AnimationClip>& clips, ECS::Entity entity)
    {
        // In Play the current clip; in Edit the Clip field at the preview time, if one is set.
        const bool        preview  = !animator.Started && animator.PreviewTime.has_value();
        const std::string& name    = animator.Started ? animator.CurrentClip : animator.Clip;
        const float        time    = animator.Started ? animator.Time : animator.PreviewTime.value_or(0.0f);
        const bool         showing = animator.Started ? animator.Playing : preview;

        const HedgehogAnimation::AnimationClip* clip = nullptr;
        if (showing && !name.empty())
        {
            clip = FindClip(clips, name);
            if (!clip && animator.WarnedClip != name)
            {
                LOGWARNING("[Animation] Entity " + std::to_string(entity) + " has no clip '" + name +
                           "' on its mesh; showing the bind pose.");
                animator.WarnedClip = name;
            }
        }

        if (clip)
            HedgehogAnimation::SamplePose(skeleton, *clip, time, animator.Loop, m_Pose);
        else
            m_Pose.assign(skeleton.BindPose.begin(), skeleton.BindPose.end());

        const std::vector<HedgehogAnimation::JointTransform>* pose = &m_Pose;
        if (clip && animator.Started && !animator.PreviousClip.empty() && animator.CrossfadeTime > 0.0f)
        {
            if (const HedgehogAnimation::AnimationClip* previous = FindClip(clips, animator.PreviousClip))
            {
                HedgehogAnimation::SamplePose(skeleton, *previous, animator.PreviousTime, animator.Loop, m_FadePose);
                HedgehogAnimation::BlendPoses(m_FadePose, m_Pose, animator.FadeElapsed / animator.CrossfadeTime,
                                              m_Blended);
                pose = &m_Blended;
            }
        }

        HedgehogAnimation::ComputeModelPose(skeleton, *pose, m_ModelPose);
        HedgehogAnimation::ComputeSkinningPalette(m_ModelPose, skeleton.InverseBind, animator.Palette);
    }
}
