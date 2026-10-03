#pragma once

#include "HedgehogScripting/api/ScriptPropertyDeclaration.hpp"
#include "HedgehogScripting/api/Sol.hpp"

#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"

#include "ECS/api/Entity.hpp"
#include "ECS/api/System.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ECS
{
    class ECS;
}

namespace FS
{
    class FileSystemManager;
}

// Callers of the save-state functions include yaml-cpp themselves.
namespace YAML
{
    class Node;
}

namespace HedgehogEngine
{
    class EngineContext;
    struct AnimationFinishedEvent;
    struct UiButtonClickedEvent;
}

namespace HedgehogScripting
{
    namespace Bindings
    {
        struct ScriptEntity;
    }

    // The script system (ADR-022): runs the Lua script of every entity with a ScriptComponent and
    // a TransformComponent. The component holds only data; every piece of runtime state lives
    // here: one sandboxed Lua state, each script file's class compiled once per Play, and each
    // entity's private environment and instance.
    //
    // A script file defines a class named after its stem (Scripts/Player.lua defines Player),
    // usually derived from ActorScript (Scripts/Base/ActorScript.lua, run once in a base
    // environment). The file's top-level globals are the class defaults: every entity gets its
    // own shallow copy of them, so `speed = 1.0` is per entity while the methods are shared.
    //
    // A script declares the values the inspector shows in a top-level `Properties` table; each
    // entity's values (its ScriptComponent's saved ones where the type matches, else the
    // declared defaults) are set on its `self` before OnStart.
    //
    // Driven by the ECS play-mode events. OnPlayStart makes an instance for every entity with a
    // script path, its property values on self. Both update events first sync the
    // scripts with the ECS: a component added during Play gets an instance, an entity that left
    // the system gets OnDisable and OnDestroy, and the component's Enable against the system's own
    // view of it gives OnStart (once, on the first enable), OnEnable and OnDisable. Then
    // OnFixedUpdate(fixedDt) or OnUpdate(dt) runs for every enabled script, so a first frame runs
    // OnStart, OnEnable, the fixed steps and then OnUpdate. Removing the component or destroying
    // the entity during Play gives OnDisable and OnDestroy (a removal callback chained in front of
    // the engine's own for the length of Play), and OnPlayStop gives them to every script.
    // Coroutines a script starts are resumed after every OnUpdate and die with their script.
    // ReloadChangedScripts swaps a running script whose file changed on disk for the new code,
    // keeping its state (ScriptSystemReload.cpp).
    //
    // A script error is logged once as "[Script] <entity> (<assets://path>): <message>" with a
    // traceback. An error in OnStart, OnEnable, OnFixedUpdate or OnUpdate faults the script: it is
    // skipped until the next Play (OnDestroy is still attempted), and the others carry on.
    class ScriptSystem : public ECS::System
    {
    public:
        // context gives the scripts' Entity API its ECS and EventBus; scriptFiles resolves
        // "assets://" script paths. Both must outlive the system.
        ScriptSystem(HedgehogEngine::EngineContext& context, const FS::FileSystemManager& scriptFiles);
        ~ScriptSystem() override;

        ScriptSystem(const ScriptSystem&)            = delete;
        ScriptSystem& operator=(const ScriptSystem&) = delete;

        void OnPlayStart(ECS::ECS& ecs) override;
        void OnPlayStop(ECS::ECS& ecs) override;
        void OnFixedUpdate(ECS::ECS& ecs, float fixedDeltaTime) override;
        void OnUpdate(ECS::ECS& ecs, float deltaTime) override;

        // Hot reload: at most once per RELOAD_POLL_INTERVAL, compares each compiled script file's
        // write time with the one it was compiled from. A changed file no running script uses is
        // dropped, so it is compiled afresh when next needed. One that running scripts use is
        // compiled again and each of its scripts swapped onto the new class: a new environment and
        // self, the old self's plain state copied over (numbers, booleans, strings, userdata such
        // as Vector3 and Entity handles, and tables of these, cycles included; functions are not),
        // the properties applied again, coroutines stopped, then OnReload() if the class has one.
        // OnStart does not run again, a faulted script gets a fresh start, and a file that no
        // longer compiles keeps the old class running and logs its error once. The Editor calls
        // it every frame; now is a parameter so tests need not wait.
        static constexpr std::chrono::milliseconds RELOAD_POLL_INTERVAL{ 1000 };
        void ReloadChangedScripts(ECS::ECS& ecs, std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());

        // Re-applies entity's ScriptComponent Properties to its script's self, for live
        // inspector edits. Does nothing outside Play or for an entity with no script.
        void PushProperties(ECS::ECS& ecs, ECS::Entity entity);

        // The script's declared Properties, sorted by name, with their defaults, for the
        // inspector. Compiles a throwaway class and runs no method.
        [[nodiscard]] std::vector<ScriptPropertyDeclaration> DescribeScript(const std::string& scriptPath);

        // Save games (ScriptSystemSaveState.cpp). The Scripts section of a save: a map from entity id
        // to { Script: <path>, Properties: { <name>: <value> }, State: <OnSave's table> } for every
        // live, healthy script, in id order. Properties are each declared property's current value
        // on self; State is what self:OnSave() returned (left out for nil; any other non-table is an
        // error). Only plain data saves: nil, booleans, numbers, strings, tables of these with string
        // or integer keys, Vector3 (!vec3), Quat (!quat) and Entity handles (!entity, the id). A
        // function, a thread, other userdata, a cyclic table, a key of another kind or nesting deeper
        // than MAX_SAVE_DEPTH leaves that script out, with one error naming it and where the value
        // is ("self.speed", "OnSave().items[2]"); the others still save.
        static constexpr int MAX_SAVE_DEPTH = 32;
        [[nodiscard]] YAML::Node SaveScriptState(ECS::ECS& ecs);

        // Restores a Scripts section. An entry whose entity runs the same script gets its saved
        // properties set on self (those still declared), then self:OnLoad(state); one whose script
        // is not running yet is kept until its instance is made (OnPlayStart, or a script added
        // during Play), which then gets them in place of OnStart. An entry for another script, or
        // one that does not read, is skipped with a warning. Kept entries are dropped at Stop.
        void LoadScriptState(ECS::ECS& ecs, const YAML::Node& section);

        // Entities with a live script instance.
        [[nodiscard]] size_t GetScriptCount() const;
        // Live Events.subscribe subscriptions, across every event name.
        [[nodiscard]] size_t GetSubscriptionCount() const;
        // Live startCoroutine coroutines, across every script.
        [[nodiscard]] size_t GetCoroutineCount() const;
        // Script files compiled since construction, the base script included.
        [[nodiscard]] int GetCompileCount() const;
        // The entity whose script is running right now, if any.
        [[nodiscard]] std::optional<ECS::Entity> GetRunningEntity() const;

        // "Scripts\\Player.lua", "Scripts/Player.lua" and "assets://Scripts/Player.lua" all name
        // "assets://Scripts/Player.lua".
        [[nodiscard]] static std::string NormalizeScriptPath(std::string_view scriptPath);

    private:
        // A compiled script file: its top-level globals and its class table.
        struct ScriptClass
        {
            sol::table                             Defaults;
            sol::table                             Class;
            std::vector<ScriptPropertyDeclaration> Declarations; // sorted by name
            std::filesystem::path                  PhysicalPath; // empty when it has none (no reload)
            std::filesystem::file_time_type        WriteTime;    // of the file this was compiled from
        };

        // What a suspended coroutine waits for before its next resume.
        enum class WakeKind
        {
            Frames,  // FramesLeft more OnUpdates
            Seconds, // m_CoroutineTime reaching WakeTime
            Until,   // Predicate returning true
        };

        // A startCoroutine coroutine: a Lua thread of the one state, owned by an EntityScript.
        struct ScriptCoroutine
        {
            uint64_t                Id = 0;
            sol::object             Thread;
            WakeKind                Wake       = WakeKind::Frames;
            double                  WakeTime   = 0.0;
            int                     FramesLeft = 0; // a new coroutine first runs at the next resume pass
            sol::protected_function Predicate;
        };

        // One entity's script instance.
        struct EntityScript
        {
            std::string                  ScriptPath;
            uint32_t                     Generation = 0;
            std::string                  EntityName;
            sol::table                   Environment;
            sol::table                   Self;
            bool                         Started = false; // OnStart has run
            bool                         Enabled = false; // the system's view of the component's Enable
            bool                         Faulted = false; // skipped until the next Play
            std::vector<ScriptCoroutine> Coroutines;      // in start order; they die with the script
        };

        // A script's Events.subscribe: the handler runs in its owner's environment. A subscription
        // with a Source (a button's onClick) gets only the events that entity raised.
        struct EventSubscription
        {
            uint64_t                Id    = 0;
            ECS::Entity             Owner = 0;
            uint32_t                Generation = 0;
            sol::protected_function Handler;
            ECS::Entity             Source           = ECS::INVALID_ENTITY;
            uint32_t                SourceGeneration = 0;
        };

        // An Events.publish, or an engine event, waiting for the end of the frame's update hook. An
        // engine event raised by an entity names it as its Source.
        struct QueuedEvent
        {
            std::string Name;
            sol::object Payload;
            ECS::Entity Source           = ECS::INVALID_ENTITY;
            uint32_t    SourceGeneration = 0;
        };

        // A loaded script state waiting for its entity's instance (LoadScriptState).
        struct PendingLoad
        {
            std::string ScriptPath;
            sol::table  Properties;
            sol::object State;
        };

        using RemovedCallback = std::function<void(ECS::Entity, HedgehogEngine::ScriptComponent&)>;

        void               StartClassSupport();
        bool               EnsureBaseLoaded();
        const ScriptClass* FindOrCompile(const std::string& scriptPath, const std::string& entityName);
        // Compiles scriptPath into a class without storing it; nullopt, logged, when it fails.
        std::optional<ScriptClass> CompileClass(const std::string& scriptPath, const std::string& entityName);
        void               CreateScript(ECS::ECS& ecs, ECS::Entity entity);
        void               SyncScripts(ECS::ECS& ecs);
        // OnDisable (if enabled and not faulted) and OnDestroy (if it has an instance), then drop it.
        void               DestroyScript(ECS::Entity entity);
        void               OnScriptRemoved(ECS::Entity entity);
        // Declared properties (ScriptSystemProperties.cpp): the Color, EntityRef and AssetRef
        // declaration helpers, reading a compiled file's `Properties`, and setting each entity's
        // values on its self, warning (when warn is set) about saved values that do not fit.
        void               RegisterPropertyHelpers();
        std::vector<ScriptPropertyDeclaration> ReadDeclarations(const sol::table& defaults,
                                                                const std::string& scriptPath) const;
        void               ApplyProperties(ECS::ECS& ecs, ECS::Entity entity, EntityScript& script, bool warn);
        // Script events (ScriptSystemEvents.cpp): the Events table, and delivery of the queued
        // events to their subscribers once every script's OnUpdate has run.
        void               RegisterEvents();
        void               DispatchEvents();
        void               DropSubscriptions(ECS::Entity owner);
        // Queues the engine's AnimationFinishedEvent as the script event "AnimationFinished",
        // payload { entity = <Entity>, clip = <name> }.
        void               QueueAnimationFinished(const HedgehogEngine::AnimationFinishedEvent& event);
        // Queues the engine's UiButtonClickedEvent as the script event "UiButtonClicked", payload
        // { entity = <Entity> }, with the button as its Source. Raised by the UI input before the
        // frame's script hooks, so handlers get it the same frame.
        void               QueueButtonClicked(const HedgehogEngine::UiButtonClickedEvent& event);
        // button:onClick(fn): a "UiButtonClicked" subscription of the running script, filtered to
        // the button. Throws outside a running script's method.
        uint64_t           SubscribeClick(const Bindings::ScriptEntity& button, sol::protected_function handler);
        // Coroutines (ScriptSystemCoroutines.cpp): startCoroutine, stopCoroutine and the wait
        // functions, and the pass that resumes the due ones once every script's OnUpdate has run.
        void               RegisterCoroutines();
        void               ResumeCoroutines();
        // Resumes one coroutine in its owner's environment and records what it waits for next;
        // a coroutine that finished or failed is removed.
        void               StepCoroutine(ECS::Entity owner, uint64_t id);
        ScriptCoroutine*   FindCoroutine(ECS::Entity owner, uint64_t id);
        void               EraseCoroutine(ECS::Entity owner, uint64_t id);
        // Hot reload (ScriptSystemReload.cpp): the Lua state copier, and the swap of one class's
        // running scripts onto its recompiled version.
        void               RegisterReload();
        void               ReloadClass(ECS::ECS& ecs, const std::string& scriptPath);
        void               SwapScript(ECS::ECS& ecs, ECS::Entity entity, const ScriptClass& scriptClass);
        // Save games (ScriptSystemSaveState.cpp): the saved properties on self, then OnLoad(state);
        // false, logged, when OnLoad fails.
        bool               ApplyLoad(ECS::Entity entity, EntityScript& script, const PendingLoad& load);
        // Runs method on every enabled, healthy script, faulting a script whose call fails.
        template<typename... Args>
        void               InvokeAll(std::string_view method, Args&&... args);
        // Calls self:method(args...) with the entity's environment current. Returns false, having
        // logged the error, when the call fails.
        template<typename... Args>
        bool Invoke(ECS::Entity entity, EntityScript& script, std::string_view method, Args&&... args);
        void LogError(const std::string& entityName, const std::string& scriptPath, const std::string& message) const;

        const FS::FileSystemManager&   m_ScriptFiles;
        HedgehogEngine::EngineContext& m_Context; // its SceneManager deletes the queued entities

        sol::state              m_Lua;
        sol::protected_function m_Traceback;
        sol::environment        m_BaseEnvironment;
        bool                    m_BaseLoaded = false;

        // Class support (see ScriptSystem.cpp): the proxy _ENV every class file runs under, and
        // the functions that switch which environment it forwards to.
        sol::environment        m_Proxy;
        sol::protected_function m_Run;
        sol::protected_function m_Invoke;
        sol::protected_function m_NewEnvironment;

        std::unordered_map<std::string, ScriptClass>  m_Classes;
        std::unordered_map<ECS::Entity, EntityScript> m_Scripts;
        std::optional<ECS::Entity>                    m_RunningEntity;
        int                                           m_CompileCount = 0;

        // While playing: the ECS whose ScriptComponent removal callback this system chained, and
        // the callback it replaced, put back at OnPlayStop.
        ECS::ECS*                                     m_CallbackEcs = nullptr;
        RemovedCallback                               m_PreviousRemovedCallback;
        // Entities removed while one of their own script calls was running; handled after it.
        std::vector<ECS::Entity>                      m_PendingRemovals;
        // Entities scripts asked to destroy, deleted once every script in the hook has run, so
        // nothing is destroyed while the scripts are being walked.
        std::vector<Bindings::ScriptEntity>           m_PendingDestroys;

        // What Time.deltaTime and Time.frame read: the current hook's dt and the OnUpdate calls
        // since Play.
        float    m_DeltaTime = 0.0f;
        uint64_t m_Frame     = 0;

        // Script events, by name, in subscription order; events published since the last dispatch.
        std::unordered_map<std::string, std::vector<EventSubscription>> m_Subscriptions;
        std::vector<QueuedEvent>                                        m_QueuedEvents;
        uint64_t                                                        m_NextSubscriptionId = 1;

        // Coroutine support: makes a thread wait can yield from, and resumes one, reporting what it
        // yielded. m_CoroutineTime is the scaled time OnUpdate has passed since Play, which wait
        // counts, so Pause freezes it and Time.timeScale scales it.
        // Copies the plain state of one self into another (see ReloadChangedScripts), and when
        // the script files were last compared with the disk.
        sol::protected_function                              m_CopyState;
        // Loaded script states waiting for their entity's instance, by entity.
        std::unordered_map<ECS::Entity, PendingLoad>         m_PendingLoads;
        std::optional<std::chrono::steady_clock::time_point> m_LastReloadPoll;

        sol::protected_function m_SpawnCoroutine;
        sol::protected_function m_StepCoroutine;
        uint64_t                m_NextCoroutineId = 1;
        double                  m_CoroutineTime   = 0.0;
    };

    // Registers the script system in the engine's ECS with its signature (ScriptComponent and
    // TransformComponent) and returns it. The one place a ScriptSystem is built: the Editor, game
    // mode and the tests all call it. scriptFiles resolves "assets://" and must outlive the
    // system; tests point it at a temporary directory.
    [[nodiscard]] std::shared_ptr<ScriptSystem> RegisterScriptSystem(HedgehogEngine::EngineContext& context,
                                                                     const FS::FileSystemManager&   scriptFiles);
}
