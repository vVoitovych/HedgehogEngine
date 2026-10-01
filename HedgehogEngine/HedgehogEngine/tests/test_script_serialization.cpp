#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "ECS/api/ECS.hpp"

#include <iostream>
#include <optional>
#include <sstream>
#include <string>

using HedgehogEngine::EngineContext;
using HedgehogEngine::FindScriptProperty;
using HedgehogEngine::ScriptComponent;
using HedgehogEngine::ScriptProperty;
using HedgehogEngine::ScriptPropertyType;
using HedgehogEngine::SceneSnapshot;

namespace
{
    // Every line Logger writes to the console while alive. Test code only.
    class ConsoleCapture
    {
    public:
        ConsoleCapture() : m_Previous(std::cout.rdbuf(m_Captured.rdbuf())) {}
        ~ConsoleCapture() { std::cout.rdbuf(m_Previous); }

        ConsoleCapture(const ConsoleCapture&)            = delete;
        ConsoleCapture& operator=(const ConsoleCapture&) = delete;

        [[nodiscard]] size_t Count(const std::string& fragment) const
        {
            std::istringstream lines(m_Captured.str());
            std::string        line;
            size_t             count = 0;
            while (std::getline(lines, line))
                if (line.find(fragment) != std::string::npos)
                    ++count;
            return count;
        }

    private:
        std::ostringstream m_Captured;
        std::streambuf*    m_Previous;
    };

    std::optional<ECS::Entity> FindScripted(ECS::ECS& ecs)
    {
        for (ECS::Entity entity = 0; entity < ECS::MAX_ENTITIES; ++entity)
            if (ecs.IsAlive(entity) && ecs.HasComponent<ScriptComponent>(entity))
                return entity;
        return std::nullopt;
    }

    // A game object whose ScriptComponent holds properties.
    ECS::Entity AddScripted(EngineContext& context, std::vector<ScriptProperty> properties)
    {
        const ECS::Entity entity = context.GetSceneManager().CreateGameObject();
        ScriptComponent   script;
        script.ScriptPath = "Scripts/Probe.lua";
        script.Properties = std::move(properties);
        context.GetECS().AddComponent(entity, script);
        return entity;
    }

    // Puts lines into snapshot's YAML right after the line holding anchor, indented as that line.
    void InsertAfter(SceneSnapshot& snapshot, const std::string& anchor, const std::vector<std::string>& lines)
    {
        const size_t at = snapshot.Yaml.find(anchor);
        REQUIRE(at != std::string::npos);
        const size_t lineStart = snapshot.Yaml.rfind('\n', at) + 1;
        const std::string indent(at - lineStart, ' ');
        std::string inserted;
        for (const std::string& line : lines)
            inserted += indent + line + "\n";
        snapshot.Yaml.insert(snapshot.Yaml.find('\n', at) + 1, inserted);
    }
}

TEST_CASE("Script serialization - Default.yaml's legacy ScriptParams load as typed properties and re-save as ScriptProperties")
{
    EngineContext context;
    const auto    scenePath = context.GetFileSystem().ResolvePhysical("assets://Scenes/Default.yaml");
    REQUIRE(scenePath.has_value());
    REQUIRE(context.GetSceneManager().LoadScene(scenePath->string()));

    const auto player = FindScripted(context.GetECS());
    REQUIRE(player.has_value());
    const auto check = [&]()
    {
        const auto& script = context.GetECS().GetComponent<ScriptComponent>(*player);
        REQUIRE(script.Properties.size() == 2);
        const ScriptProperty* speed     = FindScriptProperty(script, "speed");
        const ScriptProperty* clockWise = FindScriptProperty(script, "clockWise");
        REQUIRE(speed != nullptr);
        REQUIRE(clockWise != nullptr);
        CHECK(speed->Type == ScriptPropertyType::Number);
        CHECK(std::get<float>(speed->Value) == 7.45f);
        CHECK(clockWise->Type == ScriptPropertyType::Bool);
        CHECK(std::get<bool>(clockWise->Value) == false);
    };
    check();

    // A snapshot is the scene exactly as SaveScene writes it.
    const SceneSnapshot saved = context.GetSceneManager().CaptureSnapshot();
    CHECK(saved.Yaml.find("ScriptProperties") != std::string::npos);
    CHECK(saved.Yaml.find("ScriptParams") == std::string::npos);

    context.GetECS().GetComponent<ScriptComponent>(*player).Properties.clear();
    REQUIRE(context.GetSceneManager().RestoreSnapshot(saved));
    check();
    CHECK(context.GetSceneManager().CaptureSnapshot().Yaml == saved.Yaml);
}

TEST_CASE("Script serialization - every property type round-trips losslessly, in order")
{
    EngineContext     context;
    const ECS::Entity target = context.GetSceneManager().CreateGameObject();
    const std::vector<ScriptProperty> written = {
        { "speed", ScriptPropertyType::Number, 0.1f, {} },
        { "alive", ScriptPropertyType::Bool, true, {} },
        { "offset", ScriptPropertyType::Vector3, HM::Vector3(1.0f / 3.0f, -2.5e-7f, 123456.789f), {} },
        { "tint", ScriptPropertyType::Color, HM::Vector3(0.2f, 0.4f, 0.6f), {} },
        { "greeting", ScriptPropertyType::String, std::string("hello: \"world\" # not a comment"), {} },
        { "target", ScriptPropertyType::EntityRef, target, {} },
        { "model", ScriptPropertyType::AssetRef, std::string("Models/viking_room.obj"), "Mesh" },
    };
    const ECS::Entity entity = AddScripted(context, written);

    const SceneSnapshot saved = context.GetSceneManager().CaptureSnapshot();
    context.GetECS().GetComponent<ScriptComponent>(entity).Properties.clear();
    REQUIRE(context.GetSceneManager().RestoreSnapshot(saved));

    const auto& read = context.GetECS().GetComponent<ScriptComponent>(entity).Properties;
    REQUIRE(read.size() == written.size());
    for (size_t i = 0; i < written.size(); ++i)
    {
        CAPTURE(written[i].Name);
        CHECK(read[i].Name == written[i].Name);
        CHECK(read[i].Type == written[i].Type);
        CHECK(read[i].Value == written[i].Value);
        CHECK(read[i].AssetType == written[i].AssetType);
    }
    CHECK(context.GetSceneManager().CaptureSnapshot().Yaml == saved.Yaml);
}

TEST_CASE("Script serialization - ScriptProperties wins over ScriptParams with one warning; unreadable properties are skipped")
{
    EngineContext     context;
    const ECS::Entity entity = AddScripted(context, { { "speed", ScriptPropertyType::Number, 3.5f, {} } });
    SceneSnapshot     snapshot = context.GetSceneManager().CaptureSnapshot();

    InsertAfter(snapshot, "ScriptFile:", { "ScriptParams:", "  speed:", "    ParamType: 1", "    ParamValue: 99" });
    InsertAfter(snapshot, "ScriptProperties:", { "  broken:", "    Type: Number", "    Value: [1, 2]",
                                                 "  odd:", "    Type: Quaternion", "    Value: 1" });

    ConsoleCapture console;
    REQUIRE(context.GetSceneManager().RestoreSnapshot(snapshot));
    const auto& script = context.GetECS().GetComponent<ScriptComponent>(entity);
    REQUIRE(script.Properties.size() == 1);
    CHECK(script.Properties[0].Name == "speed");
    CHECK(std::get<float>(script.Properties[0].Value) == 3.5f);
    CHECK(console.Count("ScriptParams is ignored") == 1);
    CHECK(console.Count("property 'broken'") == 1);
    CHECK(console.Count("property 'odd'") == 1);
}
