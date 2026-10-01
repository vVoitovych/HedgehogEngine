#include "HedgehogScripting/api/ScriptSystem.hpp"

#include "Sandbox.hpp"

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Coroutines: startCoroutine(fn) returns an id owned by the running script, stopCoroutine(id)
// ends one, and inside one wait(seconds), waitFrames(n) and waitUntil(predicate) suspend it. Each
// coroutine is a Lua thread of the one state, kept in its EntityScript, and resumed by the pass
// that runs once every script's OnUpdate has run.
namespace HedgehogScripting
{
    namespace
    {
        // Leeway for float frame times adding up to a wait: 30 frames of 1/60 s are 0.5 s.
        constexpr double WAIT_TOLERANCE = 1e-6;

        // Called with debug.traceback; defines wait, waitFrames and waitUntil as globals and
        // returns spawn(fn), which makes a thread they may yield from, and step(co), which
        // resumes one and returns "dead", "error" and the message with the coroutine's
        // traceback, or "suspended" and what it waits for (a plain coroutine.yield() waits one
        // frame).
        constexpr std::string_view COROUTINE_SUPPORT = R"lua(
            local traceback = ...
            local create, resume, status = coroutine.create, coroutine.resume, coroutine.status
            local yield, running = coroutine.yield, coroutine.running
            local owned = setmetatable({}, { __mode = "k" })
            local token = {}

            local function requireCoroutine(name)
                if not owned[running()] then
                    error(name .. " can only be called inside a coroutine started with startCoroutine", 3)
                end
            end

            function wait(seconds)
                requireCoroutine("wait")
                if type(seconds) ~= "number" or seconds ~= seconds then
                    error("wait needs a number of seconds", 2)
                end
                yield(token, "seconds", seconds)
            end

            function waitFrames(frames)
                requireCoroutine("waitFrames")
                if math.type(frames) ~= "integer" then
                    error("waitFrames needs a whole number of frames", 2)
                end
                yield(token, "frames", frames)
            end

            function waitUntil(predicate)
                requireCoroutine("waitUntil")
                if type(predicate) ~= "function" then
                    error("waitUntil needs a function", 2)
                end
                yield(token, "until", predicate)
            end

            local function spawn(fn)
                local co = create(fn)
                owned[co] = true
                return co
            end

            local function step(co)
                local results = table.pack(resume(co))
                if not results[1] then
                    return "error", traceback(co, tostring(results[2]))
                end
                if status(co) == "dead" then
                    return "dead"
                end
                if results[2] == token then
                    return "suspended", results[3], results[4]
                end
                return "suspended", "frames", 1
            end

            return spawn, step
        )lua";
    }

    void ScriptSystem::RegisterCoroutines()
    {
        const std::optional<sol::protected_function> support =
            LoadScriptSource(m_Lua, COROUTINE_SUPPORT, "=script coroutine support", m_Traceback);
        if (!support)
            return;
        try
        {
            const sol::protected_function_result result = (*support)(m_Traceback);
            if (!result.valid())
            {
                const sol::error error = result;
                ReportScriptError(std::string("script coroutine support failed to start: ") + error.what());
                return;
            }
            m_SpawnCoroutine = result.get<sol::protected_function>(0);
            m_StepCoroutine  = result.get<sol::protected_function>(1);
        }
        catch (const std::exception& e)
        {
            ReportScriptError(std::string("script coroutine support failed to start: ") + e.what());
            return;
        }

        // The coroutine belongs to the entity whose script is calling; it first runs in this
        // frame's resume pass (the next frame's when started by a coroutine or event handler).
        m_Lua.set_function("startCoroutine", [this](sol::object fn)
        {
            const auto owner = m_RunningEntity.has_value() ? m_Scripts.find(*m_RunningEntity) : m_Scripts.end();
            if (owner == m_Scripts.end())
                throw std::runtime_error("startCoroutine must be called from a running script's method");
            if (fn.get_type() != sol::type::function)
                throw std::runtime_error("startCoroutine needs a function");

            const sol::protected_function_result spawned = m_SpawnCoroutine(fn);
            if (!spawned.valid())
            {
                const sol::error error = spawned;
                throw std::runtime_error(error.what());
            }
            ScriptCoroutine coroutine;
            coroutine.Id     = m_NextCoroutineId++;
            coroutine.Thread = spawned.get<sol::object>(0);
            owner->second.Coroutines.push_back(std::move(coroutine));
            return owner->second.Coroutines.back().Id;
        });

        // True when the id named a live coroutine. A coroutine may stop itself: it runs on to its
        // next wait and is never resumed again.
        m_Lua.set_function("stopCoroutine", [this](uint64_t id)
        {
            for (auto& [entity, script] : m_Scripts)
            {
                if (FindCoroutine(entity, id) != nullptr)
                {
                    EraseCoroutine(entity, id);
                    return true;
                }
            }
            return false;
        });
    }

    void ScriptSystem::ResumeCoroutines()
    {
        // A coroutine may start, stop or destroy others, so walk the ids, in entity order and
        // then start order, and look each one up again. Ones started during the pass wait for
        // the next.
        std::vector<std::pair<ECS::Entity, uint64_t>> due;
        for (const auto& [entity, script] : m_Scripts)
        {
            for (const ScriptCoroutine& coroutine : script.Coroutines)
                due.emplace_back(entity, coroutine.Id);
        }
        std::stable_sort(due.begin(), due.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

        for (const auto& [entity, id] : due)
        {
            const auto owner = m_Scripts.find(entity);
            ScriptCoroutine* coroutine = FindCoroutine(entity, id);
            // A disabled or faulted script's coroutines hold where they are.
            if (coroutine == nullptr || !owner->second.Enabled || owner->second.Faulted)
                continue;

            bool ready = false;
            switch (coroutine->Wake)
            {
            case WakeKind::Frames:
                ready = --coroutine->FramesLeft <= 0;
                break;
            case WakeKind::Seconds:
                ready = m_CoroutineTime + WAIT_TOLERANCE >= coroutine->WakeTime;
                break;
            case WakeKind::Until:
            {
                const sol::protected_function predicate   = coroutine->Predicate;
                const sol::table               environment = owner->second.Environment;
                const std::string              entityName  = owner->second.EntityName;
                const std::string              scriptPath  = owner->second.ScriptPath;

                m_RunningEntity = entity;
                try
                {
                    const sol::protected_function_result result = m_Run(environment, predicate);
                    if (result.valid())
                    {
                        // Lua truth: anything but nil and false.
                        const sol::object answer = result.get<sol::object>(0);
                        ready = answer.get_type() != sol::type::lua_nil && answer.get_type() != sol::type::none &&
                                !(answer.is<bool>() && !answer.as<bool>());
                    }
                    else
                    {
                        const sol::error error = result;
                        LogError(entityName, scriptPath, "coroutine waitUntil predicate: " + std::string(error.what()));
                        EraseCoroutine(entity, id);
                    }
                }
                catch (const std::exception& e)
                {
                    LogError(entityName, scriptPath, "coroutine waitUntil predicate: " + std::string(e.what()));
                    EraseCoroutine(entity, id);
                }
                m_RunningEntity.reset();
                break;
            }
            }
            if (ready)
                StepCoroutine(entity, id);
        }
    }

    void ScriptSystem::StepCoroutine(ECS::Entity owner, uint64_t id)
    {
        const auto       script    = m_Scripts.find(owner);
        ScriptCoroutine* coroutine = FindCoroutine(owner, id);
        if (coroutine == nullptr)
            return;

        // Copies: the coroutine may change its script's coroutines and the scripts.
        const sol::object thread      = coroutine->Thread;
        const sol::table  environment = script->second.Environment;
        const std::string entityName  = script->second.EntityName;
        const std::string scriptPath  = script->second.ScriptPath;

        m_RunningEntity = owner;
        std::string status;
        sol::object kind;
        sol::object value;
        try
        {
            const sol::protected_function_result result = m_Run(environment, m_StepCoroutine, thread);
            if (!result.valid())
            {
                const sol::error error = result;
                status                 = "error";
                LogError(entityName, scriptPath, "coroutine: " + std::string(error.what()));
            }
            else
            {
                status = result.get<std::string>(0);
                if (status == "error")
                    LogError(entityName, scriptPath, "coroutine: " + result.get<std::string>(1));
                kind  = result.get<sol::object>(1);
                value = result.get<sol::object>(2);
            }
        }
        catch (const std::exception& e)
        {
            status = "error";
            LogError(entityName, scriptPath, "coroutine: " + std::string(e.what()));
        }
        m_RunningEntity.reset();

        // Looked up again: the coroutine may have stopped itself or destroyed its own script.
        ScriptCoroutine* resumed = FindCoroutine(owner, id);
        if (resumed == nullptr)
            return;
        if (status != "suspended")
        {
            EraseCoroutine(owner, id);
            return;
        }

        const std::string wake = kind.as<std::string>();
        if (wake == "seconds")
        {
            resumed->Wake     = WakeKind::Seconds;
            resumed->WakeTime = m_CoroutineTime + value.as<double>();
        }
        else if (wake == "until")
        {
            resumed->Wake      = WakeKind::Until;
            resumed->Predicate = value.as<sol::protected_function>();
        }
        else
        {
            // waitFrames(0) still lets a frame pass: a coroutine resumes at most once per pass.
            resumed->Wake       = WakeKind::Frames;
            resumed->FramesLeft = std::max(1, value.as<int>());
        }
    }

    ScriptSystem::ScriptCoroutine* ScriptSystem::FindCoroutine(ECS::Entity owner, uint64_t id)
    {
        const auto script = m_Scripts.find(owner);
        if (script == m_Scripts.end())
            return nullptr;
        auto& coroutines = script->second.Coroutines;
        const auto it = std::find_if(coroutines.begin(), coroutines.end(), [id](const ScriptCoroutine& c) { return c.Id == id; });
        return it == coroutines.end() ? nullptr : &*it;
    }

    void ScriptSystem::EraseCoroutine(ECS::Entity owner, uint64_t id)
    {
        const auto script = m_Scripts.find(owner);
        if (script == m_Scripts.end())
            return;
        auto& coroutines = script->second.Coroutines;
        coroutines.erase(std::remove_if(coroutines.begin(), coroutines.end(), [id](const ScriptCoroutine& c) { return c.Id == id; }),
                         coroutines.end());
    }
}
