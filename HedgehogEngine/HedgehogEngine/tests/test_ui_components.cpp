#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"
#include "HedgehogEngine/api/ECS/components/UiButtonComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiCanvasComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiImageComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiRectComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiTextComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/UiSystem.hpp"

#include "ECS/api/ECS.hpp"

#include <algorithm>

using namespace HedgehogEngine;

TEST_CASE("UI components - the engine registers them, tracks canvases and keeps them through a scene snapshot")
{
    EngineContext context;
    SceneManager& scenes = context.GetSceneManager();
    ECS::ECS&     ecs    = context.GetECS();

    const ECS::Entity canvas = scenes.CreateGameObject();
    UiCanvasComponent canvasComponent;
    canvasComponent.SortOrder = 7;
    ecs.AddComponent(canvas, canvasComponent);

    const ECS::Entity button = scenes.CreateGameObject(canvas);
    UiRectComponent   rect;
    rect.Size = HM::Vector2(320.0f, 64.0f);
    ecs.AddComponent(button, rect);
    ecs.AddComponent(button, UiImageComponent{ "Textures/button.png", HM::Vector4(1.0f, 1.0f, 1.0f, 1.0f) });
    UiTextComponent text;
    text.Text = "Start";
    ecs.AddComponent(button, text);
    UiButtonComponent buttonComponent;
    buttonComponent.IsInteractable = false;
    ecs.AddComponent(button, buttonComponent);

    const auto& canvases = context.GetUiSystem()->GetEntities();
    CHECK(std::find(canvases.begin(), canvases.end(), canvas) != canvases.end());
    CHECK(std::find(canvases.begin(), canvases.end(), button) == canvases.end());

    const SceneSnapshot snapshot = scenes.CaptureSnapshot();
    for (const char* name : { "UiCanvasComponent", "UiRectComponent", "UiImageComponent", "UiTextComponent",
                              "UiButtonComponent" })
    {
        CAPTURE(name);
        CHECK(snapshot.Yaml.find(name) != std::string::npos);
    }

    ecs.RemoveComponent<UiTextComponent>(button);
    ecs.GetComponent<UiCanvasComponent>(canvas).SortOrder = 0;
    REQUIRE(scenes.RestoreSnapshot(snapshot));

    REQUIRE(ecs.HasComponent<UiTextComponent>(button));
    CHECK(ecs.GetComponent<UiTextComponent>(button).Text == "Start");
    CHECK(ecs.GetComponent<UiCanvasComponent>(canvas).SortOrder == 7);
    CHECK(ecs.GetComponent<UiRectComponent>(button).Size.x() == 320.0f);
    CHECK(ecs.GetComponent<UiImageComponent>(button).Texture == "Textures/button.png");
    CHECK_FALSE(ecs.GetComponent<UiButtonComponent>(button).IsInteractable);
    CHECK(scenes.CaptureSnapshot().Yaml == snapshot.Yaml);
}
