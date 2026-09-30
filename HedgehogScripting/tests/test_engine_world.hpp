#pragma once

#include "HedgehogScripting/api/ScriptSystem.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/HierarchySystem.hpp"
#include "HedgehogEngine/api/ECS/systems/TransformSystem.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"
#include "HedgehogEngine/api/Simulation/Simulation.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/api/PathUtils.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include "doctest/doctest/doctest.h"

#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace ScriptingTest
{
    using namespace HedgehogEngine;
    using HedgehogScripting::ScriptSystem;

    inline constexpr float STEP = 1.0f / 60.0f;

    // A script class named name, derived from ActorScript, with the given methods.
    inline std::string ScriptClass(const std::string& name, const std::string& methods)
    {
        return name + " = setmetatable({}, { __index = ActorScript })\n" + name + ".__index = " + name + "\n" +
               "function " + name + ":new() return setmetatable(ActorScript:new(), " + name + ") end\n" + methods;
    }

    inline std::string ShippedBaseScript()
    {
        std::ifstream     file(FS::GetEngineRootDirectory() / "Assets" / "Scripts" / "Base" / "ActorScript.lua");
        std::stringstream text;
        text << file.rdbuf();
        return text.str();
    }

    // The engine's ECS, scenes, Simulation and transform systems, with a ScriptSystem over a
    // temp "assets://" holding the test scripts.
    struct EngineWorld
    {
        TempDir                        Dir;
        FS::FileSystemManager          ScriptFiles;
        EngineContext                  Context;
        std::shared_ptr<ScriptSystem> Scripts;
        std::vector<std::string>       Log;

        EngineWorld()
        {
            Dir.WriteFile("Scripts/Base/ActorScript.lua", ShippedBaseScript());
            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("assets://", Dir.Path());
            REQUIRE(ScriptFiles.Register(std::move(fs)));

            Scripts = ScriptSystem::Register(Context.GetECS(), Context.GetEventBus(), ScriptFiles);
            Scripts->GetVM().GetState().set_function("Record", [this](const std::string& event) { Log.push_back(event); });
        }

        ECS::ECS&     Ecs() { return Context.GetECS(); }
        SceneManager& Scenes() { return Context.GetSceneManager(); }
        Simulation&   Sim() { return Context.GetSimulation(); }

        ECS::Entity AddObject(const std::string& name, ECS::Entity parent)
        {
            const ECS::Entity entity = Scenes().CreateGameObject(parent);
            Ecs().GetComponent<ECS::HierarchyComponent>(entity).Name = name;
            return entity;
        }

        ECS::Entity AddScripted(const std::string& name, const std::string& className, const std::string& methods)
        {
            Dir.WriteFile("Scripts/" + className + ".lua", ScriptClass(className, methods));
            const ECS::Entity entity = AddObject(name, Ecs().GetRoot());
            ScriptComponent   script;
            script.ScriptPath = "Scripts/" + className + ".lua";
            Ecs().AddComponent(entity, script);
            return entity;
        }

        // What EngineContext::UpdateContext runs after the Simulation.
        void UpdateTransforms()
        {
            Context.GetTransformSystem()->Update(Ecs(), Context.GetEventBus());
            Context.GetHierarchySystem()->Update(Ecs(), Context.GetEventBus());
        }

        HM::Vector3 WorldPosition(ECS::Entity entity)
        {
            const HM::Vector4& translation = Ecs().GetComponent<TransformComponent>(entity).ObjMatrix[3];
            return HM::Vector3(translation.x(), translation.y(), translation.z());
        }
    };

    inline bool Near(const HM::Vector3& a, const HM::Vector3& b)
    {
        return (a - b).LengthSqr() < 1e-8f;
    }
}
