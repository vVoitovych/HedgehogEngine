#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogEngine/api/ECS/components/UiButtonComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiCanvasComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiImageComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiRectComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiTextComponent.hpp"

#include "ECS/api/components/Hierarchy.hpp"

#include <string>

using namespace HedgehogEngine;

namespace
{
    constexpr float     STEP      = 1.0f / 60.0f;
    const HM::Vector2   VIEW_SIZE = HM::Vector2(800.0f, 600.0f);

    // A script class named `name` with the given OnStart and OnUpdate bodies (empty ones are left out).
    std::string Script(const std::string& name, const std::string& onStart, const std::string& onUpdate = "")
    {
        std::string source = name + " = setmetatable({}, { __index = ActorScript })\n" + name + ".__index = " + name +
                             "\n" + "function " + name + ":new() return setmetatable(ActorScript:new(), " + name +
                             ") end\n";
        if (!onStart.empty())
            source += "function " + name + ":OnStart()\n" + onStart + "\nend\n";
        if (!onUpdate.empty())
            source += "function " + name + ":OnUpdate(dt)\n" + onUpdate + "\nend\n";
        return source;
    }

    // A constant-pixel-size canvas.
    ECS::Entity Canvas(EngineWorld& world)
    {
        UiCanvasComponent canvas;
        canvas.ScaleMode = UiCanvasScaleMode::ConstantPixelSize;
        const ECS::Entity entity = world.Context.GetSceneManager().CreateGameObject();
        world.Ecs().GetComponent<ECS::HierarchyComponent>(entity).Name = "Canvas";
        world.Ecs().AddComponent(entity, canvas);
        return entity;
    }

    // A named element at (x, y), 100x50 pixels, under parent.
    ECS::Entity Element(EngineWorld& world, ECS::Entity parent, const std::string& name, float x, float y)
    {
        UiRectComponent rect;
        rect.AnchorMin = HM::Vector2(0.0f, 0.0f);
        rect.AnchorMax = HM::Vector2(0.0f, 0.0f);
        rect.Pivot     = HM::Vector2(0.0f, 0.0f);
        rect.Offset    = HM::Vector2(x, y);
        rect.Size      = HM::Vector2(100.0f, 50.0f);
        const ECS::Entity entity = world.Context.GetSceneManager().CreateGameObject(parent);
        world.Ecs().GetComponent<ECS::HierarchyComponent>(entity).Name = name;
        world.Ecs().AddComponent(entity, rect);
        return entity;
    }

    ECS::Entity Button(EngineWorld& world, ECS::Entity canvas, const std::string& name, float x, float y)
    {
        const ECS::Entity entity = Element(world, canvas, name, x, y);
        world.Ecs().AddComponent(entity, UiImageComponent{});
        world.Ecs().AddComponent(entity, UiButtonComponent{});
        return entity;
    }

    HW::RawInput Pointer(float x, float y, bool down = false)
    {
        HW::RawInput input;
        input.CursorPosition = HM::Vector2(x, y);
        input.CursorInside   = true;
        input.CursorKnown    = true;
        input.MouseButtons[static_cast<size_t>(HW::MouseButton::Left)] = down;
        return input;
    }

    // A full click at (x, y): over it, pressed, released.
    void Click(EngineWorld& world, float x, float y)
    {
        world.Frame(STEP, Pointer(x, y), VIEW_SIZE);
        world.Frame(STEP, Pointer(x, y, true), VIEW_SIZE);
        world.Frame(STEP, Pointer(x, y), VIEW_SIZE);
    }
}

TEST_CASE("UI bindings - a script updates a score text each frame and toggles a panel")
{
    EngineWorld       world;
    const ECS::Entity canvas = Canvas(world);
    const ECS::Entity score  = Element(world, canvas, "Score", 0.0f, 0.0f);
    world.Ecs().AddComponent(score, UiTextComponent{});
    const ECS::Entity panel = Element(world, canvas, "Panel", 0.0f, 100.0f);
    world.Ecs().AddComponent(panel, UiImageComponent{});

    world.WriteScript("Hud.lua", Script("Hud", R"lua(
    score = 0
    text = Scene.find("Score"):getUiText()
    panel = Scene.find("Panel"):getUiRect()
    image = Scene.find("Panel"):getUiImage()
    image.texture = "assets://Textures\\panel.png"
    image.color = Vector3(1, 0, 0)
    image.alpha = 0.5
    text.fontSize = 32
    text.font = "Fonts/Karla-Regular.ttf"
    panel.offsetX = 20
    panel.height = 80
)lua",
                                        R"lua(
    score = score + 10
    text.text = "Score: " .. score
    text.color = Vector3(0, 1, 0)
    panel.visible = not panel.visible
)lua"));
    (void)world.AddScripted("Scripts/Hud.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(world.Ecs().GetComponent<UiTextComponent>(score).Text == "Score: 10");
    CHECK_FALSE(world.Ecs().GetComponent<UiRectComponent>(panel).IsVisible);
    world.Frame(STEP);
    world.Frame(STEP);
    CHECK(world.Ecs().GetComponent<UiTextComponent>(score).Text == "Score: 30");
    CHECK_FALSE(world.Ecs().GetComponent<UiRectComponent>(panel).IsVisible);
    world.Frame(STEP);
    CHECK(world.Ecs().GetComponent<UiRectComponent>(panel).IsVisible);

    const UiTextComponent& text = world.Ecs().GetComponent<UiTextComponent>(score);
    CHECK(text.FontSize == 32.0f);
    CHECK(text.Font == "Fonts/Karla-Regular.ttf");
    CHECK(text.Color == HM::Vector4(0.0f, 1.0f, 0.0f, 1.0f));
    const UiImageComponent& image = world.Ecs().GetComponent<UiImageComponent>(panel);
    CHECK(image.Texture == "Textures/panel.png");
    CHECK(image.Color == HM::Vector4(1.0f, 0.0f, 0.0f, 0.5f));
    const UiRectComponent& rect = world.Ecs().GetComponent<UiRectComponent>(panel);
    CHECK(rect.Offset == HM::Vector2(20.0f, 100.0f));
    CHECK(rect.Size == HM::Vector2(100.0f, 80.0f));
    CHECK(log.Lines("[ERROR]").empty());

    REQUIRE(world.Stop());
    CHECK(world.Ecs().GetComponent<UiTextComponent>(score).Text.empty()); // Stop restores the scene
}

TEST_CASE("UI bindings - onClick fires once per click and goes with its script")
{
    EngineWorld       world;
    const ECS::Entity canvas = Canvas(world);
    (void)Button(world, canvas, "Play", 100.0f, 100.0f);
    (void)Button(world, canvas, "Quit", 300.0f, 100.0f);

    world.WriteScript("Menu.lua", Script("Menu", R"lua(
    local play = Scene.find("Play"):getUiButton()
    play:onClick(function(event) print("clicked " .. event.entity.name) end)
    Events.subscribe("UiButtonClicked", function(event) print("any " .. event.entity.name) end)
)lua",
                                         R"lua(
    if Scene.find("Play"):getUiButton():isPressed() then print("pressed") end
)lua"));
    const ECS::Entity menu = world.AddScripted("Scripts/Menu.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP); // OnStart subscribes
    CHECK(world.Scripts->GetSubscriptionCount() == 2);

    Click(world, 150.0f, 120.0f);
    CHECK(log.Lines("clicked Play").size() == 1);
    CHECK(log.Lines("any Play").size() == 1);
    CHECK(log.Lines("pressed").size() == 1); // the press frame only

    Click(world, 350.0f, 120.0f); // the other button: only the general subscription hears it
    CHECK(log.Lines("clicked").size() == 1);
    CHECK(log.Lines("any Quit").size() == 1);

    Click(world, 150.0f, 120.0f);
    CHECK(log.Lines("clicked Play").size() == 2);

    // The script's entity goes: its subscriptions go with it, and later clicks reach nothing.
    world.Context.GetSceneManager().DeleteGameObject(menu);
    world.Frame(STEP);
    CHECK(world.Scripts->GetSubscriptionCount() == 0);
    Click(world, 150.0f, 120.0f);
    CHECK(log.Lines("clicked Play").size() == 2);
    CHECK(log.Lines("[ERROR]").empty());
    REQUIRE(world.Stop());
}

TEST_CASE("UI bindings - onClick is released at Stop, and a click before Play reaches no one")
{
    EngineWorld       world;
    const ECS::Entity canvas = Canvas(world);
    (void)Button(world, canvas, "Play", 100.0f, 100.0f);
    world.WriteScript("Menu.lua", Script("Menu", R"lua(
    id = Scene.find("Play"):getUiButton():onClick(function() print("clicked") end)
    print("unsubscribed " .. tostring(Events.unsubscribe(id)))
    Scene.find("Play"):getUiButton():onClick(function() print("clicked") end)
)lua"));
    (void)world.AddScripted("Scripts/Menu.lua");

    LogCapture log;
    Click(world, 150.0f, 120.0f); // Edit mode
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(log.Lines("unsubscribed true").size() == 1);
    CHECK(world.Scripts->GetSubscriptionCount() == 1);
    REQUIRE(world.Stop());
    CHECK(world.Scripts->GetSubscriptionCount() == 0);
    CHECK(log.Lines("clicked").empty());
}

TEST_CASE("UI bindings - getX, hasX and addX, and bad values are script errors")
{
    EngineWorld       world;
    const ECS::Entity canvas = Canvas(world);
    world.WriteScript("Builder.lua", Script("Builder", R"lua(
    local label = Scene.spawn("Label", Scene.find("Canvas"))
    print("has " .. tostring(label:hasUiText()) .. " " .. tostring(label:getUiText()))
    label:addUiRect().width = 300
    local text = label:addUiText()
    text.text = "Hello"
    print("same " .. tostring(label:addUiText().text))
    local button = label:addUiButton()
    button.interactable = false
    print("hovered " .. tostring(button:isHovered()) .. " " .. tostring(button))
    label:addUiImage().texture = ""
)lua",
                                            R"lua(
    if not tried then
        tried = true
        Scene.find("Label"):getUiText().fontSize = 0
    end
)lua"));
    world.WriteScript("Escape.lua", Script("Escape", "    Scene.find(\"Label\"):getUiImage().texture = \"../secret.png\""));
    world.WriteScript("Outside.lua", Script("Outside", "    Scene.find(\"Canvas\"):addUiButton():onClick(function() end)"));
    (void)world.AddScripted("Scripts/Builder.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(log.Lines("has false nil").size() == 1);
    CHECK(log.Lines("same Hello").size() == 1);
    CHECK(log.Lines("hovered false UiButton of Entity").size() == 1);
    CHECK(log.Lines("a font size must be above 0").size() == 1);

    // Later scripts: an image path leaving assets://, and onClick from a script works anywhere it runs.
    (void)world.AddScripted("Scripts/Escape.lua");
    (void)world.AddScripted("Scripts/Outside.lua");
    world.Frame(STEP);
    CHECK(log.Lines("a texture path may not leave assets://").size() == 1);
    CHECK(world.Scripts->GetSubscriptionCount() == 1);

    const auto& children = world.Ecs().GetComponent<ECS::HierarchyComponent>(canvas).Children;
    REQUIRE(children.size() == 1);
    const ECS::Entity label = children[0];
    CHECK(world.Ecs().GetComponent<UiRectComponent>(label).Size.x() == 300.0f);
    CHECK(world.Ecs().GetComponent<UiTextComponent>(label).Text == "Hello");
    CHECK_FALSE(world.Ecs().GetComponent<UiButtonComponent>(label).IsInteractable);
    REQUIRE(world.Stop());
}
