#pragma once

#include "HedgehogScripting/api/ScriptSystem.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "ECS/api/ECS.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include <memory>
#include <string>
#include <unordered_map>

// The engine as the Editor runs it, plus the script system registered the way the Editor will
// register it, reading scripts from "assets://" mounted on a fresh temp directory. The base
// ActorScript is copied in unchanged from the shipped Assets. Test code only.
class EngineWorld
{
public:
    EngineWorld()
    {
        auto fs = std::make_unique<FS::FileSystem>();
        fs->RegisterPath("assets://", m_Dir.Path());
        m_ScriptFiles.Register(std::move(fs));

        const auto shippedBase = Context.GetFileSystem().ReadTextFile("assets://Scripts/Base/ActorScript.lua");
        if (shippedBase)
            m_Dir.WriteFile("Scripts/Base/ActorScript.lua", *shippedBase);

        Scripts = HedgehogScripting::RegisterScriptSystem(Context, m_ScriptFiles);
    }

    EngineWorld(const EngineWorld&)            = delete;
    EngineWorld& operator=(const EngineWorld&) = delete;

    // Writes Scripts/<name> under the temp "assets://".
    void WriteScript(const std::string& name, const std::string& source) const
    {
        m_Dir.WriteFile("Scripts/" + name, source);
    }

    // A new game object (Transform and Hierarchy) running scriptPath.
    ECS::Entity AddScripted(const std::string& scriptPath, bool enabled = true)
    {
        const ECS::Entity entity = Context.GetSceneManager().CreateGameObject();
        HedgehogEngine::ScriptComponent script;
        script.ScriptPath = scriptPath;
        script.Enable     = enabled;
        Context.GetECS().AddComponent(entity, script);
        m_ScriptPaths[entity] = scriptPath;
        return entity;
    }

    // EngineContext::Stop, then the script paths this fixture assigned put back. Stop's scene
    // restore re-initializes every ScriptComponent through the legacy script system, which is
    // still registered until the switch-over: it looks for the script in the real Assets folder,
    // logs "[Lua Error] cannot open", and blanks the path of one it cannot find there.
    bool Stop()
    {
        const bool stopped = Context.Stop();
        for (const auto& [entity, path] : m_ScriptPaths)
        {
            if (Context.GetECS().IsAlive(entity) && Context.GetECS().HasComponent<HedgehogEngine::ScriptComponent>(entity))
                Context.GetECS().GetComponent<HedgehogEngine::ScriptComponent>(entity).ScriptPath = path;
        }
        return stopped;
    }

    ECS::ECS& Ecs() { return Context.GetECS(); }

private:
    TempDir               m_Dir;
    FS::FileSystemManager m_ScriptFiles;
    std::unordered_map<ECS::Entity, std::string> m_ScriptPaths;

public:
    HedgehogEngine::EngineContext                   Context;
    std::shared_ptr<HedgehogScripting::ScriptSystem> Scripts;
};
