#include "HedgehogEngine/api/ECS/systems/AnimationSystem.hpp"

#include "HedgehogEngine/api/Containers/Mesh.hpp"
#include "HedgehogEngine/api/Containers/MeshContainer.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/Events/AnimationEvents.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"

#include "HedgehogAnimation/api/BlendStack.hpp"
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

        std::optional<uint32_t> FindClipIndex(const std::vector<HedgehogAnimation::AnimationClip>& clips,
                                              const std::string& name)
        {
            for (size_t index = 0; index < clips.size(); ++index)
                if (clips[index].Name == name)
                    return static_cast<uint32_t>(index);
            return std::nullopt;
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
            animator.Started = false;
            animator.Playing = false;
            animator.CurrentClip.clear();
            HedgehogAnimation::ClearBlendStack(animator.Blend);
            animator.BlendMesh.reset();
            animator.RequestedFade.reset();
            animator.RequestedLoop.reset();
            animator.CurrentLoop.reset();
            animator.Finished = false;
        }

        bool CurrentLoopOf(const AnimatorComponent& animator)
        {
            return animator.CurrentLoop.value_or(animator.Loop);
        }

        void WarnMissingClip(AnimatorComponent& animator, ECS::Entity entity, const std::string& name)
        {
            if (animator.WarnedClip == name)
                return;
            LOGWARNING("[Animation] Entity " + std::to_string(entity) + " has no clip '" + name +
                       "' on its mesh; showing the bind pose.");
            animator.WarnedClip = name;
        }

        // Puts CurrentClip on the blend stack from its start, fading in over fade seconds (alone
        // when 0). Without the mesh's clips (it is not loaded yet) the stack stays empty until they
        // come; a name the mesh lacks shows the bind pose and warns once.
        void StartCurrentClip(AnimatorComponent& animator, const std::vector<HedgehogAnimation::AnimationClip>* clips,
                              float fade, ECS::Entity entity)
        {
            const std::optional<uint32_t> index = clips ? FindClipIndex(*clips, animator.CurrentClip) : std::nullopt;
            if (!index)
            {
                HedgehogAnimation::ClearBlendStack(animator.Blend);
                if (clips)
                    WarnMissingClip(animator, entity, animator.CurrentClip);
                return;
            }
            HedgehogAnimation::PushClip(animator.Blend, *index, 1.0f, CurrentLoopOf(animator), fade);
        }

        // Publishes the end of a non-looping clip, once per play-through.
        void PublishFinished(AnimatorComponent& animator, const std::vector<HedgehogAnimation::AnimationClip>& clips,
                             ECS::Entity entity, EventBus& bus)
        {
            if (!animator.Playing || animator.Finished || animator.CurrentClip.empty())
                return;
            const HedgehogAnimation::BlendEntry* current = HedgehogAnimation::GetCurrentEntry(animator.Blend);
            if (!current || current->Loop || current->Clip >= clips.size() || current->Time < clips[current->Clip].Duration)
                return;
            animator.Finished = true;
            bus.Publish(AnimationFinishedEvent{ entity, animator.CurrentClip });
        }

        std::optional<uint64_t> MeshIndexOf(const ECS::ECS& ecs, ECS::Entity entity)
        {
            return ecs.HasComponent<MeshComponent>(entity) ? ecs.GetComponent<MeshComponent>(entity).MeshIndex
                                                           : std::nullopt;
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
            const Mesh*        loaded   = FindSkinnedMesh(ecs, meshes, entity);
            const std::vector<HedgehogAnimation::AnimationClip>* clips =
                loaded ? &loaded->GetAnimationClips() : nullptr;
            const std::optional<uint64_t> meshIndex = loaded ? MeshIndexOf(ecs, entity) : std::nullopt;

            if (playing)
            {
                // The stack's clip indices name another mesh's clips (it loaded late, or changed):
                // the current clip starts again on this one.
                if (animator.Started && animator.BlendMesh != meshIndex)
                {
                    animator.BlendMesh = meshIndex;
                    if (animator.Playing && !animator.CurrentClip.empty())
                        StartCurrentClip(animator, clips, 0.0f, entity);
                    else
                        HedgehogAnimation::ClearBlendStack(animator.Blend);
                }
                Advance(animator, clips, meshIndex, dt, entity);
            }
            else if (animator.Started)
            {
                ResetPlayState(animator);
            }

            if (!loaded)
            {
                animator.Palette.clear();
                continue;
            }
            if (playing)
                PublishFinished(animator, *clips, entity, bus);
            Evaluate(animator, *loaded->GetSkeleton(), *clips, entity);
        }
    }

    bool AnimationSystem::Play(ECS::ECS& ecs, const MeshContainer& meshes, ECS::Entity entity, const std::string& clip,
                               std::optional<float> fade, std::optional<bool> loop)
    {
        AnimatorComponent& animator = ecs.GetComponent<AnimatorComponent>(entity);
        const Mesh*        loaded   = FindSkinnedMesh(ecs, meshes, entity);
        if (!loaded || !FindClipIndex(loaded->GetAnimationClips(), clip))
        {
            LOGWARNING("[Animation] Entity " + std::to_string(entity) + " has no clip '" + clip + "' on its mesh; " +
                       (animator.CurrentClip.empty() ? std::string("nothing plays.")
                                                     : "keeping '" + animator.CurrentClip + "'."));
            return false;
        }

        animator.Clip          = clip;
        animator.RequestedFade = fade;
        animator.RequestedLoop = loop;
        animator.Finished      = false;
        if (!animator.Started)
        {
            // Before the animator's first frame: start with this clip, as PlayOnStart would.
            animator.Started     = true;
            animator.Playing     = true;
            animator.CurrentClip = clip;
            animator.CurrentLoop = loop;
            animator.BlendMesh   = MeshIndexOf(ecs, entity);
            animator.RequestedFade.reset();
            animator.RequestedLoop.reset();
            StartCurrentClip(animator, &loaded->GetAnimationClips(), 0.0f, entity);
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

    float AnimationSystem::GetTime(const AnimatorComponent& animator)
    {
        const HedgehogAnimation::BlendEntry* current = HedgehogAnimation::GetCurrentEntry(animator.Blend);
        return current ? current->Time : 0.0f;
    }

    void AnimationSystem::SetTime(AnimatorComponent& animator, float time)
    {
        if (HedgehogAnimation::BlendEntry* current = HedgehogAnimation::GetCurrentEntry(animator.Blend))
            current->Time = time;
        animator.Finished = false;
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

    void AnimationSystem::Advance(AnimatorComponent& animator, const std::vector<HedgehogAnimation::AnimationClip>* clips,
                                  std::optional<uint64_t> meshIndex, float dt, ECS::Entity entity)
    {
        if (!animator.Started)
        {
            animator.Started     = true;
            animator.Playing     = animator.PlayOnStart && !animator.Clip.empty();
            animator.CurrentClip = animator.Playing ? animator.Clip : std::string{};
            animator.CurrentLoop.reset();
            animator.BlendMesh = meshIndex;
            HedgehogAnimation::ClearBlendStack(animator.Blend);
            if (animator.Playing)
                StartCurrentClip(animator, clips, 0.0f, entity);
            return; // the first frame shows time 0
        }

        if (animator.Clip != animator.CurrentClip)
        {
            // A new clip plays from its start; what played fades out from where it was, unless
            // nothing was playing or there is no fade time (Play's, else CrossfadeTime).
            const float fadeTime = animator.RequestedFade.value_or(animator.CrossfadeTime);
            const bool  fade     = animator.Playing && !animator.CurrentClip.empty() && fadeTime > 0.0f;
            animator.RequestedFade.reset();
            animator.CurrentLoop = animator.RequestedLoop;
            animator.RequestedLoop.reset();
            animator.Finished    = false;
            animator.CurrentClip = animator.Clip;
            animator.Playing     = !animator.Clip.empty();
            animator.BlendMesh   = meshIndex;
            if (animator.Playing)
                StartCurrentClip(animator, clips, fade ? fadeTime : 0.0f, entity);
            else
                HedgehogAnimation::ClearBlendStack(animator.Blend);
            return; // the new clip shows at time 0 this frame
        }

        if (!animator.Playing)
            return;
        // Loop applies to the current clip live (a script may change it); a clip fading out
        // keeps the loop it played with.
        if (HedgehogAnimation::BlendEntry* current = HedgehogAnimation::GetCurrentEntry(animator.Blend))
            current->Loop = CurrentLoopOf(animator);
        HedgehogAnimation::AdvanceBlendStack(animator.Blend, dt * animator.Speed, dt);
    }

    void AnimationSystem::Evaluate(AnimatorComponent& animator, const HedgehogAnimation::Skeleton& skeleton,
                                   const std::vector<HedgehogAnimation::AnimationClip>& clips, ECS::Entity entity)
    {
        if (animator.Started)
        {
            // In Play the blend stack: empty when nothing plays, so the bind pose.
            HedgehogAnimation::EvaluateBlendStack(animator.Blend, skeleton, clips, m_Scratch, m_Pose);
        }
        else
        {
            // In Edit the Clip field at the preview time, if one is set, else the bind pose.
            const HedgehogAnimation::AnimationClip* clip = nullptr;
            if (animator.PreviewTime && !animator.Clip.empty())
            {
                clip = FindClip(clips, animator.Clip);
                if (!clip)
                    WarnMissingClip(animator, entity, animator.Clip);
            }
            if (clip)
                HedgehogAnimation::SamplePose(skeleton, *clip, *animator.PreviewTime, animator.Loop, m_Pose);
            else
                m_Pose.assign(skeleton.BindPose.begin(), skeleton.BindPose.end());
        }

        HedgehogAnimation::ComputeModelPose(skeleton, m_Pose, m_ModelPose);
        HedgehogAnimation::ComputeSkinningPalette(m_ModelPose, skeleton.InverseBind, animator.Palette);
    }
}
