#include "HedgehogEngine/api/ECS/systems/AnimationSystem.hpp"

#include "HedgehogEngine/api/Containers/Mesh.hpp"
#include "HedgehogEngine/api/Containers/MeshContainer.hpp"
#include "HedgehogEngine/api/ECS/components/CharacterControllerComponent.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/Events/AnimationEvents.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"

#include "HedgehogAnimation/api/BlendStack.hpp"
#include "HedgehogAnimation/api/BlendTree.hpp"
#include "HedgehogAnimation/api/Pose.hpp"
#include "HedgehogAnimation/api/TwoBoneIk.hpp"

#include "FileSystem/api/FileSystemManager.hpp"
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
            // The controller's file stays cached; its machine starts afresh on the next Play.
            animator.ControllerState.Ready   = false;
            animator.ControllerState.Running = false;
            animator.ControllerState.RequestedState.clear();
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

        // RootJoint's index on the skeleton: joint 0 when empty, none when the skeleton lacks it.
        std::optional<uint32_t> FindRootJoint(const AnimatorComponent& animator, const HedgehogAnimation::Skeleton& skeleton)
        {
            if (skeleton.JointNames.empty())
                return std::nullopt;
            if (animator.RootJoint.empty())
                return 0u;
            for (size_t joint = 0; joint < skeleton.JointNames.size(); ++joint)
                if (skeleton.JointNames[joint] == animator.RootJoint)
                    return static_cast<uint32_t>(joint);
            return std::nullopt;
        }

        // The rotation's twist about Y, the mesh's up.
        HM::Quaternion TwistAboutY(const HM::Quaternion& rotation)
        {
            const HM::Quaternion twist(0.0f, rotation.y(), 0.0f, rotation.w());
            return twist.Length() > 1e-6f ? twist.Normalize() : HM::Quaternion::Identity();
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
        m_Files   = ecs.GetServices().Find<FS::FileSystemManager>();
    }

    void AnimationSystem::OnUnregister(ECS::ECS& /*ecs*/)
    {
        m_Bus     = nullptr;
        m_Catalog = nullptr;
        m_Files   = nullptr;
    }

    void AnimationSystem::OnFrame(ECS::ECS& ecs, const ECS::FrameContext& ctx)
    {
        if (!m_Bus || !m_Catalog)
        {
            return;
        }
        // The same scaled time OnUpdate got; nothing advances while paused.
        const float dt = ctx.Mode == ECS::PlayMode::Playing ? ctx.ScaledDeltaTime : 0.0f;
        ReloadChangedControllers(std::chrono::steady_clock::now());
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

            m_RootSkeleton = nullptr;
            if (playing && animator.ApplyRootMotion && loaded)
            {
                if (const std::optional<uint32_t> root = FindRootJoint(animator, *loaded->GetSkeleton()))
                {
                    m_RootSkeleton = loaded->GetSkeleton();
                    m_RootJoint    = *root;
                }
                else if (!animator.WarnedRootJoint)
                {
                    animator.WarnedRootJoint = true;
                    LOGWARNING("[Animation] Entity " + std::to_string(entity) + " has no root joint '" +
                               animator.RootJoint + "' on its mesh; it applies no root motion.");
                }
            }
            m_RootMotion = HedgehogAnimation::RootMotionDelta{};
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
                Advance(animator, clips, meshIndex, dt, entity, &bus);
            }
            else if (animator.Started)
            {
                ResetPlayState(animator);
            }
            if (m_RootSkeleton)
                ApplyRootMotion(ecs, entity, animator, dt);
            else
            {
                animator.RootMotion        = HM::Vector3(0.0f, 0.0f, 0.0f);
                animator.PendingRootMotion = HM::Vector3(0.0f, 0.0f, 0.0f);
            }

            if (!loaded)
            {
                animator.Palette.clear();
                continue;
            }
            if (playing)
                PublishFinished(animator, *clips, entity, bus);
            const HM::Matrix4x4 world = ecs.HasComponent<TransformComponent>(entity)
                                            ? ecs.GetComponent<TransformComponent>(entity).ObjMatrix
                                            : HM::Matrix4x4::GetIdentity();
            Evaluate(animator, *loaded->GetSkeleton(), *clips, entity, world);
        }
    }

    bool AnimationSystem::Play(ECS::ECS& ecs, const MeshContainer& meshes, ECS::Entity entity, const std::string& clip,
                               std::optional<float> fade, std::optional<bool> loop)
    {
        AnimatorComponent& animator = ecs.GetComponent<AnimatorComponent>(entity);
        if (const HedgehogAnimation::AnimatorController* controller = FindController(animator))
        {
            // With a controller a name is a state, entered at the next frame.
            if (HedgehogAnimation::FindState(*controller, clip) < 0)
            {
                LOGWARNING("[Animation] Entity " + std::to_string(entity) + "'s controller " + animator.Controller +
                           " has no state '" + clip + "'; nothing changes.");
                return false;
            }
            animator.ControllerState.RequestedState = clip;
            animator.RequestedFade                  = fade;
            return true;
        }
        const Mesh* loaded = FindSkinnedMesh(ecs, meshes, entity);
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
        if (animator.ControllerState.Running)
        {
            // The machine holds its state and parameters until Play names a state.
            animator.Playing  = false;
            animator.Finished = false;
            animator.CurrentClip.clear();
            HedgehogAnimation::ClearBlendStack(animator.Blend);
            return;
        }
        const bool started = animator.Started;
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
                                  std::optional<uint64_t> meshIndex, float dt, ECS::Entity entity, EventBus* bus)
    {
        // A controller that reads, validates and finds every clip on the mesh picks the clips;
        // otherwise Clip plays.
        if (clips)
        {
            if (const HedgehogAnimation::AnimatorController* controller = FindController(animator);
                controller && PrepareController(animator, *controller, *clips, meshIndex, entity))
            {
                AdvanceController(animator, *controller, *clips, meshIndex, dt, entity, bus);
                return;
            }
        }

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
        AdvanceBlend(animator, clips, dt * animator.Speed, dt);
    }

    void AnimationSystem::AdvanceBlend(AnimatorComponent& animator, const std::vector<HedgehogAnimation::AnimationClip>* clips,
                                       float clipDt, float dt)
    {
        if (m_RootSkeleton && clips)
            HedgehogAnimation::AdvanceBlendStack(animator.Blend, clipDt, dt, *m_RootSkeleton, *clips, m_RootJoint, m_RootMotion);
        else
            HedgehogAnimation::AdvanceBlendStack(animator.Blend, clipDt, dt);
    }

    void AnimationSystem::ApplyRootMotion(ECS::ECS& ecs, ECS::Entity entity, AnimatorComponent& animator, float dt)
    {
        if (!ecs.HasComponent<TransformComponent>(entity))
            return;
        // The default mask: the root's movement along the mesh's X and Z, and its turn about Y.
        const HM::Vector3    moved(m_RootMotion.Translation.x(), 0.0f, m_RootMotion.Translation.z());
        const HM::Quaternion turn      = TwistAboutY(m_RootMotion.Rotation);
        TransformComponent&  transform = ecs.GetComponent<TransformComponent>(entity);
        const HM::Vector4    world     = transform.ObjMatrix * HM::Vector4(moved.x(), moved.y(), moved.z(), 0.0f);
        animator.RootMotion            = HM::Vector3(world.x(), world.y(), world.z());

        const HM::Quaternion rotation = HM::Quaternion::FromEuler(transform.Rotation);
        bool                 changed  = false;
        if (ecs.HasComponent<CharacterControllerComponent>(entity))
            animator.PendingRootMotion = dt > 0.0f ? animator.RootMotion * (1.0f / dt) : HM::Vector3(0.0f, 0.0f, 0.0f);
        else if (moved.LengthSlow() > 0.0f)
        {
            // The mesh's space is the entity's local space: scaled and turned into its parent's.
            const HM::Vector3 scaled(moved.x() * transform.Scale.x(), moved.y() * transform.Scale.y(),
                                     moved.z() * transform.Scale.z());
            transform.Position = transform.Position + rotation * scaled;
            changed            = true;
        }
        if (std::abs(turn.y()) > 1e-7f)
        {
            transform.Rotation = (rotation * turn).Normalize().ToEuler();
            changed            = true;
        }
        if (changed && m_Bus)
            m_Bus->Publish(TransformChangedEvent{ entity });
    }

    void AnimationSystem::PreviewBlendTree(const HedgehogAnimation::AnimatorController& controller,
                                           const HedgehogAnimation::AnimatorState& state,
                                           const HedgehogAnimation::Skeleton& skeleton,
                                           const std::vector<HedgehogAnimation::AnimationClip>& clips, float time,
                                           std::vector<HedgehogAnimation::JointTransform>& outPose)
    {
        // The tree at its parameters' defaults, its children found on the mesh (one it lacks left out).
        const auto parameter = [&](const std::string& name)
        {
            const int index = HedgehogAnimation::FindParameter(controller, name);
            return index >= 0 ? controller.Parameters[static_cast<size_t>(index)].Default : 0.0f;
        };
        std::array<float, HedgehogAnimation::MAX_BLEND_TREE_CHILDREN> weights{};
        HedgehogAnimation::ComputeBlendTreeWeights(state, parameter(state.BlendParameterX), parameter(state.BlendParameterY),
                                                   weights);
        std::array<HedgehogAnimation::BlendEntryChild, HedgehogAnimation::MAX_BLEND_TREE_CHILDREN> children{};
        uint32_t count = 0;
        for (size_t c = 0; c < state.Children.size() && c < children.size(); ++c)
        {
            for (size_t i = 0; i < clips.size(); ++i)
            {
                if (clips[i].Name == state.Children[c].Clip)
                {
                    children[count++] = { static_cast<uint32_t>(i), weights[c], clips[i].Duration, state.Children[c].Speed };
                    break;
                }
            }
        }
        HedgehogAnimation::BlendStack stack;
        HedgehogAnimation::PushBlendTree(stack, std::span(children.data(), count), state.Speed, state.Loop, 0.0f);
        // From the start of its cycle, PreviewTime seconds on.
        HedgehogAnimation::AdvanceBlendStack(stack, std::max(0.0f, time) / std::max(state.Speed, 1e-6f), 0.0f);
        HedgehogAnimation::EvaluateBlendStack(stack, skeleton, clips, m_Scratch, outPose);
    }

    bool AnimationSystem::SetIkGoal(ECS::ECS& ecs, ECS::Entity entity, const std::string& joint, const HM::Vector3& target,
                                    float weight, const std::optional<HM::Vector3>& pole)
    {
        const auto finite = [](const HM::Vector3& v) { return std::isfinite(v.x()) && std::isfinite(v.y()) && std::isfinite(v.z()); };
        if (!ecs.HasComponent<AnimatorComponent>(entity) || joint.empty() || !(weight >= 0.0f && weight <= 1.0f) ||
            !finite(target) || (pole && !finite(*pole)))
            return false;
        std::vector<AnimatorIkGoal>& goals = ecs.GetComponent<AnimatorComponent>(entity).IkGoals;
        const auto found = std::find_if(goals.begin(), goals.end(), [&](const AnimatorIkGoal& goal) { return goal.Joint == joint; });
        if (found != goals.end())
        {
            found->Target = target;
            found->Weight = weight;
            found->Pole   = pole;
            return true;
        }
        if (goals.size() >= MAX_IK_GOALS)
            return false;
        goals.push_back(AnimatorIkGoal{ joint, target, weight, pole });
        return true;
    }

    bool AnimationSystem::ClearIkGoal(ECS::ECS& ecs, ECS::Entity entity, const std::string& joint)
    {
        if (!ecs.HasComponent<AnimatorComponent>(entity))
            return false;
        return std::erase_if(ecs.GetComponent<AnimatorComponent>(entity).IkGoals,
                             [&](const AnimatorIkGoal& goal) { return goal.Joint == joint; }) > 0;
    }

    void AnimationSystem::ApplyIkGoals(AnimatorComponent& animator, const HedgehogAnimation::Skeleton& skeleton,
                                       ECS::Entity entity, const HM::Matrix4x4& world)
    {
        const HM::Matrix4x4 toMesh  = world.Inverse();
        const auto          inMesh  = [&](const HM::Vector3& point)
        {
            const HM::Vector4 local = toMesh * HM::Vector4(point.x(), point.y(), point.z(), 1.0f);
            return HM::Vector3(local.x(), local.y(), local.z());
        };
        for (AnimatorIkGoal& goal : animator.IkGoals)
        {
            const auto joint = std::find(skeleton.JointNames.begin(), skeleton.JointNames.end(), goal.Joint);
            const std::optional<HedgehogAnimation::TwoBoneChain> chain =
                joint == skeleton.JointNames.end()
                    ? std::nullopt
                    : HedgehogAnimation::FindTwoBoneChain(skeleton,
                                                          static_cast<uint32_t>(joint - skeleton.JointNames.begin()));
            if (!chain)
            {
                if (!goal.Warned)
                    LOGWARNING("[Animation] Entity " + std::to_string(entity) + "'s IK goal names joint '" + goal.Joint +
                               "', which its skeleton lacks or which has fewer than two ancestors; it is skipped.");
                goal.Warned = true;
                continue;
            }
            const std::optional<HM::Vector3> pole = goal.Pole ? std::optional(inMesh(*goal.Pole)) : std::nullopt;
            HedgehogAnimation::SolveTwoBoneIk(skeleton, *chain, inMesh(goal.Target), pole, goal.Weight, m_Pose, m_ModelPose);
        }
    }

    void AnimationSystem::Evaluate(AnimatorComponent& animator, const HedgehogAnimation::Skeleton& skeleton,
                                   const std::vector<HedgehogAnimation::AnimationClip>& clips, ECS::Entity entity,
                                   const HM::Matrix4x4& world)
    {
        if (animator.Started)
        {
            // In Play the blend stack: empty when nothing plays, so the bind pose.
            HedgehogAnimation::EvaluateBlendStack(animator.Blend, skeleton, clips, m_Scratch, m_Pose);
            if (animator.ApplyRootMotion)
                if (const std::optional<uint32_t> root = FindRootJoint(animator, skeleton))
                    HedgehogAnimation::RemoveRootMotion(m_Pose, *root, skeleton.BindPose);
        }
        else
        {
            // In Edit the Clip field (with a controller, its default state's clip) at the preview
            // time, if one is set, else the bind pose.
            const HedgehogAnimation::AnimationClip* clip          = nullptr;
            bool                                    previewedTree = false;
            if (animator.PreviewTime)
            {
                std::string name = animator.Clip;
                if (const HedgehogAnimation::AnimatorController* controller = FindController(animator))
                {
                    const HedgehogAnimation::AnimatorState& state = controller->States[controller->DefaultState];
                    if (state.Motion != HedgehogAnimation::StateMotion::Clip)
                    {
                        PreviewBlendTree(*controller, state, skeleton, clips, *animator.PreviewTime, m_Pose);
                        previewedTree = true;
                        name.clear();
                    }
                    else
                        name = state.Clip;
                }
                if (!name.empty())
                {
                    clip = FindClip(clips, name);
                    if (!clip)
                        WarnMissingClip(animator, entity, name);
                }
            }
            if (clip)
                HedgehogAnimation::SamplePose(skeleton, *clip, *animator.PreviewTime, animator.Loop, m_Pose);
            else if (!previewedTree)
                m_Pose.assign(skeleton.BindPose.begin(), skeleton.BindPose.end());
        }

        HedgehogAnimation::ComputeModelPose(skeleton, m_Pose, m_ModelPose);
        if (!animator.IkGoals.empty())
            ApplyIkGoals(animator, skeleton, entity, world);
        HedgehogAnimation::ComputeSkinningPalette(m_ModelPose, skeleton.InverseBind, animator.Palette);
    }
}
