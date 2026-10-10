#include "HedgehogEngine/api/ECS/systems/AnimationSystem.hpp"

#include "HedgehogEngine/api/Events/AnimationEvents.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"

#include "HedgehogAnimation/api/AnimatorControllerFile.hpp"
#include "HedgehogAnimation/api/BlendStack.hpp"

#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/api/PathUtils.hpp"

#include "Logger/api/Logger.hpp"

#include <system_error>

// The animator controllers AnimationSystem runs: their files, cached and re-read when they change,
// and how a controller's machine drives an animator's blend stack.
namespace HedgehogEngine
{
    namespace
    {
        std::filesystem::file_time_type WriteTimeOf(const FS::FileSystemManager& files, const std::string& virtualPath)
        {
            const std::optional<std::filesystem::path> physical = files.ResolvePhysical(virtualPath);
            std::error_code                            error;
            return physical ? std::filesystem::last_write_time(*physical, error) : std::filesystem::file_time_type{};
        }

        float StateSpeed(const HedgehogAnimation::AnimatorController& controller,
                         const HedgehogAnimation::StateMachineInstance& machine)
        {
            const HedgehogAnimation::AnimatorState& state = controller.States[machine.State];
            float                                   speed = state.Speed;
            if (!state.SpeedParameter.empty())
            {
                if (const std::optional<float> value = HedgehogAnimation::GetParameter(controller, machine, state.SpeedParameter))
                    speed *= *value;
            }
            return speed;
        }

        // The machine starts from the defaults the first time a controller's parameters are used.
        void EnsureMachine(AnimatorControllerRuntime& runtime, const HedgehogAnimation::AnimatorController& controller)
        {
            if (runtime.Ready)
                return;
            HedgehogAnimation::ResetStateMachine(controller, runtime.Machine);
            runtime.Ready = true;
        }
    }

    void AnimationSystem::LoadController(ControllerSlot& slot)
    {
        const std::string path = FS::ToAssetVirtualPath(slot.Key);
        slot.WriteTime         = WriteTimeOf(*m_Files, path);
        slot.Controller.reset();
        ++slot.Version;

        const std::optional<std::string> text = m_Files->ReadTextFile(path);
        if (!text)
        {
            LOGERROR("[Animation] " + path + " cannot be read; its animators play their Clip.");
            return;
        }
        HedgehogAnimation::AnimatorControllerParseResult parsed = HedgehogAnimation::ParseAnimatorController(*text);
        if (!parsed.Controller)
        {
            LOGERROR("[Animation] " + path + ": " + parsed.Error + "; its animators play their Clip.");
            return;
        }
        const std::vector<std::string> problems = HedgehogAnimation::ValidateAnimatorController(*parsed.Controller);
        if (!problems.empty())
        {
            for (const std::string& problem : problems)
                LOGERROR("[Animation] " + path + ": " + problem + ".");
            LOGERROR("[Animation] " + path + " cannot run; its animators play their Clip.");
            return;
        }
        slot.Controller = std::move(*parsed.Controller);
    }

    void AnimationSystem::ReloadChangedControllers(std::chrono::steady_clock::time_point now)
    {
        if (!m_Files || m_Controllers.empty() || now - m_LastControllerPoll < CONTROLLER_POLL_INTERVAL)
            return;
        m_LastControllerPoll = now;
        for (ControllerSlot& slot : m_Controllers)
        {
            if (WriteTimeOf(*m_Files, FS::ToAssetVirtualPath(slot.Key)) == slot.WriteTime)
                continue;
            LoadController(slot);
            LOGINFO("[Animation] Reloaded " + FS::ToAssetVirtualPath(slot.Key) + ".");
        }
    }

    const HedgehogAnimation::AnimatorController* AnimationSystem::FindController(AnimatorComponent& animator)
    {
        AnimatorControllerRuntime& runtime = animator.ControllerState;
        if (animator.Controller.empty() || !m_Files)
        {
            if (!runtime.Path.empty() || runtime.Slot >= 0)
                runtime = AnimatorControllerRuntime{};
            return nullptr;
        }

        if (runtime.Path != animator.Controller)
        {
            // Another file: its own slot, read once for every animator naming it however spelt.
            runtime      = AnimatorControllerRuntime{};
            runtime.Path = animator.Controller;
            const std::string key = FS::MakeAssetKey(animator.Controller);
            for (size_t i = 0; i < m_Controllers.size(); ++i)
            {
                if (m_Controllers[i].Key == key)
                    runtime.Slot = static_cast<int32_t>(i);
            }
            if (runtime.Slot < 0)
            {
                m_Controllers.push_back(ControllerSlot{ key });
                LoadController(m_Controllers.back());
                runtime.Slot = static_cast<int32_t>(m_Controllers.size() - 1);
            }
        }

        const ControllerSlot& slot = m_Controllers[static_cast<size_t>(runtime.Slot)];
        if (runtime.Version != slot.Version)
        {
            // A new version (or the first): its machine starts afresh.
            runtime.Version = slot.Version;
            runtime.Ready   = false;
            runtime.Running = false;
            runtime.Mesh.reset();
            runtime.Durations.clear();
            runtime.Usable = false;
        }
        return slot.Controller ? &*slot.Controller : nullptr;
    }

    bool AnimationSystem::PrepareController(AnimatorComponent& animator, const HedgehogAnimation::AnimatorController& controller,
                                            const std::vector<HedgehogAnimation::AnimationClip>& clips,
                                            std::optional<uint64_t> meshIndex, ECS::Entity entity)
    {
        AnimatorControllerRuntime& runtime = animator.ControllerState;
        if (runtime.Mesh == meshIndex && runtime.Durations.size() == controller.States.size())
            return runtime.Usable;

        runtime.Mesh = meshIndex;
        runtime.Durations.assign(controller.States.size(), 0.0f);
        runtime.Usable = true;
        for (size_t state = 0; state < controller.States.size(); ++state)
        {
            const std::string& clip  = controller.States[state].Clip;
            bool               found = false;
            for (const HedgehogAnimation::AnimationClip& candidate : clips)
            {
                if (candidate.Name == clip)
                {
                    runtime.Durations[state] = candidate.Duration;
                    found                    = true;
                    break;
                }
            }
            if (!found)
            {
                LOGERROR("[Animation] Entity " + std::to_string(entity) + ": state '" + controller.States[state].Name +
                         "' of " + FS::ToAssetVirtualPath(FS::MakeAssetKey(animator.Controller)) + " plays clip '" + clip +
                         "', which its mesh does not have; it plays its Clip.");
                runtime.Usable = false;
            }
        }
        return runtime.Usable;
    }

    void AnimationSystem::EnterState(AnimatorComponent& animator, const HedgehogAnimation::AnimatorController& controller,
                                     const std::vector<HedgehogAnimation::AnimationClip>* clips, uint32_t state, float fade,
                                     ECS::Entity entity, EventBus* bus, const std::string& from)
    {
        AnimatorControllerRuntime& runtime = animator.ControllerState;
        runtime.Machine.State              = state;
        runtime.Machine.StateTime          = 0.0f;
        runtime.Running                    = true;

        const HedgehogAnimation::AnimatorState& entered = controller.States[state];
        animator.Playing     = true;
        animator.Finished    = false;
        animator.CurrentClip = entered.Clip;
        animator.CurrentLoop = entered.Loop;
        std::optional<uint32_t> index;
        if (clips)
        {
            for (size_t i = 0; i < clips->size(); ++i)
                if ((*clips)[i].Name == entered.Clip)
                    index = static_cast<uint32_t>(i);
        }
        if (index)
            HedgehogAnimation::PushClip(animator.Blend, *index, StateSpeed(controller, runtime.Machine), entered.Loop, fade);
        else
            HedgehogAnimation::ClearBlendStack(animator.Blend);

        if (bus)
            bus->Publish(AnimatorStateChangedEvent{ entity, from, entered.Name });
    }

    void AnimationSystem::AdvanceController(AnimatorComponent& animator, const HedgehogAnimation::AnimatorController& controller,
                                            const std::vector<HedgehogAnimation::AnimationClip>& clips,
                                            std::optional<uint64_t> meshIndex, float dt, ECS::Entity entity, EventBus* bus)
    {
        AnimatorControllerRuntime& runtime = animator.ControllerState;
        EnsureMachine(runtime, controller);
        animator.BlendMesh = meshIndex;

        const auto requested = [&]() -> std::optional<uint32_t>
        {
            if (runtime.RequestedState.empty())
                return std::nullopt;
            const int state = HedgehogAnimation::FindState(controller, runtime.RequestedState);
            runtime.RequestedState.clear();
            return state >= 0 ? std::optional<uint32_t>(static_cast<uint32_t>(state)) : std::nullopt;
        };

        if (!animator.Started || !runtime.Running)
        {
            // The first frame, or a controller new to it (assigned, or its file re-read): the
            // default state, or the one Play asked for, from its start.
            animator.Started = true;
            HedgehogAnimation::ClearBlendStack(animator.Blend);
            EnterState(animator, controller, &clips, requested().value_or(controller.DefaultState), 0.0f, entity, bus, {});
            animator.RequestedFade.reset();
            return; // the state shows at time 0 this frame
        }

        if (const std::optional<uint32_t> state = requested())
        {
            const std::string from = controller.States[runtime.Machine.State].Name;
            const float       fade = animator.RequestedFade.value_or(animator.CrossfadeTime);
            animator.RequestedFade.reset();
            EnterState(animator, controller, &clips, *state, animator.Playing ? fade : 0.0f, entity, bus, from);
            return;
        }
        if (!animator.Playing)
            return; // stopped: it waits for Play

        // The state's speed (its parameter may change) and loop apply to its clip live.
        if (HedgehogAnimation::BlendEntry* current = HedgehogAnimation::GetCurrentEntry(animator.Blend))
        {
            current->Speed = StateSpeed(controller, runtime.Machine);
            current->Loop  = controller.States[runtime.Machine.State].Loop;
        }
        const float clipDt = dt * animator.Speed;
        HedgehogAnimation::AdvanceBlendStack(animator.Blend, clipDt, dt);

        const uint32_t from = runtime.Machine.State;
        if (const std::optional<HedgehogAnimation::TransitionFired> fired =
                HedgehogAnimation::StepStateMachine(controller, runtime.Machine, runtime.Durations, clipDt))
            EnterState(animator, controller, &clips, fired->To, fired->Duration, entity, bus, controller.States[from].Name);
    }

    const HedgehogAnimation::AnimatorController* AnimationSystem::GetController(ECS::ECS& ecs, ECS::Entity entity)
    {
        if (!ecs.HasComponent<AnimatorComponent>(entity))
            return nullptr;
        return FindController(ecs.GetComponent<AnimatorComponent>(entity));
    }

    bool AnimationSystem::SetFloat(ECS::ECS& ecs, ECS::Entity entity, const std::string& name, float value)
    {
        return SetParameter(ecs, entity, [&](const HedgehogAnimation::AnimatorController& controller,
                                             HedgehogAnimation::StateMachineInstance& machine)
                            { return HedgehogAnimation::SetFloat(controller, machine, name, value); });
    }

    bool AnimationSystem::SetInt(ECS::ECS& ecs, ECS::Entity entity, const std::string& name, int value)
    {
        return SetParameter(ecs, entity, [&](const HedgehogAnimation::AnimatorController& controller,
                                             HedgehogAnimation::StateMachineInstance& machine)
                            { return HedgehogAnimation::SetInt(controller, machine, name, value); });
    }

    bool AnimationSystem::SetBool(ECS::ECS& ecs, ECS::Entity entity, const std::string& name, bool value)
    {
        return SetParameter(ecs, entity, [&](const HedgehogAnimation::AnimatorController& controller,
                                             HedgehogAnimation::StateMachineInstance& machine)
                            { return HedgehogAnimation::SetBool(controller, machine, name, value); });
    }

    bool AnimationSystem::SetTrigger(ECS::ECS& ecs, ECS::Entity entity, const std::string& name, bool set)
    {
        return SetParameter(ecs, entity, [&](const HedgehogAnimation::AnimatorController& controller,
                                             HedgehogAnimation::StateMachineInstance& machine)
                            { return HedgehogAnimation::SetTrigger(controller, machine, name, set); });
    }

    bool AnimationSystem::SetParameter(ECS::ECS& ecs, ECS::Entity entity, const ParameterSetter& set)
    {
        if (!ecs.HasComponent<AnimatorComponent>(entity))
            return false;
        AnimatorComponent& animator = ecs.GetComponent<AnimatorComponent>(entity);
        const HedgehogAnimation::AnimatorController* controller = FindController(animator);
        if (!controller)
            return false;
        EnsureMachine(animator.ControllerState, *controller);
        return set(*controller, animator.ControllerState.Machine);
    }

    std::optional<float> AnimationSystem::GetParameter(ECS::ECS& ecs, ECS::Entity entity, const std::string& name)
    {
        if (!ecs.HasComponent<AnimatorComponent>(entity))
            return std::nullopt;
        AnimatorComponent& animator = ecs.GetComponent<AnimatorComponent>(entity);
        const HedgehogAnimation::AnimatorController* controller = FindController(animator);
        if (!controller)
            return std::nullopt;
        EnsureMachine(animator.ControllerState, *controller);
        return HedgehogAnimation::GetParameter(*controller, animator.ControllerState.Machine, name);
    }

    std::string AnimationSystem::GetStateName(ECS::ECS& ecs, ECS::Entity entity)
    {
        if (!ecs.HasComponent<AnimatorComponent>(entity))
            return {};
        AnimatorComponent& animator = ecs.GetComponent<AnimatorComponent>(entity);
        const HedgehogAnimation::AnimatorController* controller = FindController(animator);
        if (!controller || !animator.ControllerState.Running || animator.ControllerState.Machine.State >= controller->States.size())
            return {};
        return controller->States[animator.ControllerState.Machine.State].Name;
    }
}
