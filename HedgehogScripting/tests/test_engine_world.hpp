#pragma once

#include "HedgehogScripting/api/ScriptSystem.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"
#include "HedgehogEngine/api/ECS/systems/HierarchySystem.hpp"
#include "HedgehogEngine/api/ECS/systems/TransformSystem.hpp"

#include "ECS/api/ECS.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>

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

    // Writes Scripts/<name> again and moves its write time a few seconds on, so a hot-reload
    // poll sees the change however coarse the file system's clock is.
    void RewriteScript(const std::string& name, const std::string& source) const
    {
        const std::filesystem::path path   = m_Dir.Path() / "Scripts" / name;
        const auto                  before = std::filesystem::last_write_time(path);
        m_Dir.WriteFile("Scripts/" + name, source);
        std::filesystem::last_write_time(path, before + std::chrono::seconds(5));
    }

    // A new game object (Transform and Hierarchy) running scriptPath.
    ECS::Entity AddScripted(const std::string& scriptPath, bool enabled = true)
    {
        const ECS::Entity entity = Context.GetSceneManager().CreateGameObject();
        HedgehogEngine::ScriptComponent script;
        script.ScriptPath = scriptPath;
        script.Enable     = enabled;
        Context.GetECS().AddComponent(entity, script);
        return entity;
    }

    bool Stop() { return Context.Stop(); }

    ECS::ECS& Ecs() { return Context.GetECS(); }

    // One frame with the game's input: the input is evaluated first, as the Editor does before
    // UpdateContext, then Frame(dt). The UI takes input only over a game view with an area.
    void Frame(float dt, const HW::RawInput& gameInput, const HM::Vector2& gameViewSize = HM::Vector2(0.0f, 0.0f))
    {
        Context.UpdateGameInput(gameInput, gameViewSize);
        Frame(dt);
    }

    // One frame as EngineContext::UpdateContext runs it, less the editor camera (which needs a
    // window): gameplay, then Transform and Hierarchy, so script writes reach ObjMatrix.
    void Frame(float dt)
    {
        Context.UpdatePlayMode(dt);
        Context.GetTransformSystem()->Update(Context.GetECS(), Context.GetEventBus());
        Context.GetHierarchySystem()->Update(Context.GetECS(), Context.GetEventBus());
    }

private:
    TempDir               m_Dir;
    FS::FileSystemManager m_ScriptFiles;

public:
    HedgehogEngine::EngineContext                   Context;
    std::shared_ptr<HedgehogScripting::ScriptSystem> Scripts;
};
