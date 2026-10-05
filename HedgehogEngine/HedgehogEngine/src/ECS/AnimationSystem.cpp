#include "HedgehogEngine/api/ECS/systems/AnimationSystem.hpp"

#include "HedgehogEngine/api/Containers/Mesh.hpp"
#include "HedgehogEngine/api/Containers/MeshContainer.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/Events/AnimationEvents.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"

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

        // The entity's loaded mesh, if it has one with a skeleton.
        const Mesh* FindSkinnedMesh(const ECS::ECS& ecs, const MeshContainer& meshes, ECS::Entity entity)
        {
            if (!ecs.HasComponent<MeshComponent>(entity))
                return nullptr;
            const MeshComponent& mesh = ecs.GetComponent<MeshComponent>(entity);
            if (!mesh.MeshIndex || *mesh.MeshIndex >= meshes.GetMeshCount())
                return nullptr;
            const Mesh& loaded = meshes.GetMesh(static_cast<size_t>(*mesh.MeshIndex));
            return loaded.GetSkeleton() ? &loaded : nullptr;
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
            animator.FadeDuration = 0.0f;
            animator.RequestedFade.reset();
            animator.Finished     = false;
        }

        // Publishes the end of a non-looping clip, once per play-through.
        void PublishFinished(AnimatorComponent& animator, const std::vector<HedgehogAnimation::AnimationClip>& clips,
                             ECS::Entity entity, EventBus& bus)
        {
            if (!animator.Playing || animator.Loop || animator.Finished || animator.CurrentClip.empty())
                return;
            const HedgehogAnimation::AnimationClip* clip = FindClip(clips, animator.CurrentClip);
            if (!clip || animator.Time < clip->Duration)
                return;
            animator.Finished = true;
            bus.Publish(AnimationFinishedEvent{ entity, animator.CurrentClip });
        }
    }

    void AnimationSystem::OnRegister(ECS::ECS& ecs)
    {
        m_Bus     = ecs.GetServices().Find<EventBus>();
        m_Catalog = ecs.GetServices().Find<ResourceCatalog>();
    }

    void AnimationSystem::OnUnregister(ECS::ECS& /*ecs*/)
    {
        m_Bus     = nullptr;
        m_Catalog = nullptr;
    }

    void AnimationSystem::OnFrame(ECS::ECS& ecs, const ECS::FrameContext& ctx)
    {
        if (!m_Bus || !m_Catalog)
        {
            return;
        }
        // The same scaled time OnUpdate got; nothing advances while paused.
        const float dt = ctx.Mode == ECS::PlayMode::Playing ? ctx.ScaledDeltaTime : 0.0f;
        Update(ecs, m_Catalog->GetMeshContainer(), *m_Bus, ctx.Mode != ECS::PlayMode::Edit, dt);
    }

    void AnimationSystem::Update(ECS::ECS& ecs, const MeshContainer& meshes, EventBus& bus, bool playing, float dt)
    {
        for (const ECS::Entity entity : m_Entities)
        {
            AnimatorComponent& animator = ecs.GetComponent<AnimatorComponent>(entity);

            if (playing)
                Advance(animator, dt);
            else if (animator.Started)
                ResetPlayState(animator);

            const Mesh* loaded = FindSkinnedMesh(ecs, meshes, entity);
            if (!loaded)
            {
                animator.Palette.clear();
                continue;
            }
            if (playing)
                PublishFinished(animator, loaded->GetAnimationClips(), entity, bus);
            Evaluate(animator, *loaded->GetSkeleton(), loaded->GetAnimationClips(), entity);
        }
    }

    bool AnimationSystem::Play(ECS::ECS& ecs, const MeshContainer& meshes, ECS::Entity entity, const std::string& clip,
                               std::optional<float> fade)
    {
        AnimatorComponent& animator = ecs.GetComponent<AnimatorComponent>(entity);
        const Mesh*        loaded   = FindSkinnedMesh(ecs, meshes, entity);
        if (!loaded || !FindClip(loaded->GetAnimationClips(), clip))
        {
            LOGWARNING("[Animation] Entity " + std::to_string(entity) + " has no clip '" + clip + "' on its mesh; " +
                       (animator.CurrentClip.empty() ? std::string("nothing plays.")
                                                     : "keeping '" + animator.CurrentClip + "'."));
            return false;
        }

        animator.Clip          = clip;
        animator.RequestedFade = fade;
        animator.Finished      = false;
        if (!animator.Started)
        {
            // Before the animator's first frame: start with this clip, as PlayOnStart would.
            animator.Started     = true;
            animator.Playing     = true;
            animator.CurrentClip = clip;
            animator.Time        = 0.0f;
            animator.RequestedFade.reset();
        }
        else if (animator.CurrentClip == clip)
        {
            // Restart: the next Advance sees a new clip with nothing to fade out of.
            animator.CurrentClip.clear();
        }
        return true;
    }

    void AnimationSystem::Stop(ECS::ECS& ecs, ECS::Entity entity)
    {
        AnimatorComponent& animator = ecs.GetComponent<AnimatorComponent>(entity);
        const bool         started  = animator.Started;
        ResetPlayState(animator);
        animator.Started = started; // in Play, an empty Clip keeps it stopped
        animator.Clip.clear();
    }

    std::vector<std::string> AnimationSystem::GetClipNames(const ECS::ECS& ecs, const MeshContainer& meshes,
                                                           ECS::Entity entity) const
    {
        std::vector<std::string> names;
        if (const Mesh* loaded = FindSkinnedMesh(ecs, meshes, entity))
        {
            for (const HedgehogAnimation::AnimationClip& clip : loaded->GetAnimationClips())
                names.push_back(clip.Name);
        }
        return names;
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
            // nothing was playing or there is no fade time (Play's, else CrossfadeTime).
            const float fadeTime = animator.RequestedFade.value_or(animator.CrossfadeTime);
            const bool  fade     = animator.Playing && !animator.CurrentClip.empty() && fadeTime > 0.0f;
            animator.RequestedFade.reset();
            animator.FadeDuration = fade ? fadeTime : 0.0f;
            animator.Finished     = false;
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
            if (animator.FadeElapsed >= animator.FadeDuration)
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
        if (clip && animator.Started && !animator.PreviousClip.empty() && animator.FadeDuration > 0.0f)
        {
            if (const HedgehogAnimation::AnimationClip* previous = FindClip(clips, animator.PreviousClip))
            {
                HedgehogAnimation::SamplePose(skeleton, *previous, animator.PreviousTime, animator.Loop, m_FadePose);
                HedgehogAnimation::BlendPoses(m_FadePose, m_Pose, animator.FadeElapsed / animator.FadeDuration,
                                              m_Blended);
                pose = &m_Blended;
            }
        }

        HedgehogAnimation::ComputeModelPose(skeleton, *pose, m_ModelPose);
        HedgehogAnimation::ComputeSkinningPalette(m_ModelPose, skeleton.InverseBind, animator.Palette);
    }
}
