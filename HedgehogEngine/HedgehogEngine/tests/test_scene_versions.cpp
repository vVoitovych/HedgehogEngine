#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "EcsSerialization/api/EcsSerializer.hpp"

#include <optional>
#include <string>

using HedgehogEngine::EngineContext;

TEST_CASE("Scene versions - every shipped scene, saved before versioning, loads as version 1 and saves with a Version")
{
    const std::string version = "Version: " + std::to_string(EcsSerialization::EcsSerializer::FORMAT_VERSION) + "\n";
    for (const char* scene : { "Default.yaml", "Animated.yaml", "Hud.yaml", "benchmark.yaml", "test.yaml" })
    {
        CAPTURE(scene);
        EngineContext context;
        const std::string virtualPath = std::string("assets://Scenes/") + scene;

        const std::optional<std::string> text = context.GetFileSystem().ReadTextFile(virtualPath);
        REQUIRE(text.has_value());
        CHECK(text->find("Version:") == std::string::npos); // the fixture: written before versioning

        const auto physical = context.GetFileSystem().ResolvePhysical(virtualPath);
        REQUIRE(physical.has_value());
        REQUIRE(context.GetSceneManager().LoadScene(physical->string()));
        CHECK(context.GetSceneManager().CaptureSnapshot().Yaml.starts_with(version));
    }
}
