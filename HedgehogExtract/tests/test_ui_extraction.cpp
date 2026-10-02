#include "HedgehogExtract/api/RenderScene.hpp"
#include "HedgehogExtract/api/SceneExtractor.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"
#include "HedgehogEngine/api/ECS/components/RenderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiButtonComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiCanvasComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiImageComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiRectComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiTextComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/UiSystem.hpp"

#include "doctest/doctest/doctest.h"

#include <memory>
#include <string>
#include <vector>

using namespace HedgehogEngine;

namespace
{
    const HM::Vector2 TARGET(800.0f, 600.0f);

    // An ECS with the hierarchy, the UI components and UiSystem, as EngineContext registers them.
    struct UiFixture
    {
        ECS::ECS                  ecs;
        std::shared_ptr<UiSystem> uiSystem;

        UiFixture()
        {
            ecs.Init();
            ecs.RegisterComponent<ECS::HierarchyComponent>();
            ecs.RegisterComponent<RenderComponent>();
            ecs.RegisterComponent<UiCanvasComponent>();
            ecs.RegisterComponent<UiRectComponent>();
            ecs.RegisterComponent<UiImageComponent>();
            ecs.RegisterComponent<UiTextComponent>();
            ecs.RegisterComponent<UiButtonComponent>();
            uiSystem = ecs.RegisterSystem<UiSystem>();
            ECS::Signature signature;
            signature.set(ecs.GetComponentType<UiCanvasComponent>());
            ecs.SetSystemSignature<UiSystem>(signature);
        }

        ECS::Entity AddNode(ECS::Entity parent)
        {
            const ECS::Entity entity = ecs.CreateEntity();
            ecs.AddComponent(entity, ECS::HierarchyComponent{ "node", parent, {} });
            if (parent != ECS::INVALID_ENTITY)
                ecs.GetComponent<ECS::HierarchyComponent>(parent).Children.push_back(entity);
            return entity;
        }

        // A canvas at one pixel per unit unless the test changes it.
        ECS::Entity AddCanvas(int32_t sortOrder = 0)
        {
            const ECS::Entity  entity = AddNode(ECS::INVALID_ENTITY);
            UiCanvasComponent canvas;
            canvas.ScaleMode = UiCanvasScaleMode::ConstantPixelSize;
            canvas.SortOrder = sortOrder;
            ecs.AddComponent(entity, canvas);
            return entity;
        }

        // An element pinned at its parent's top-left, with an image unless image is false.
        ECS::Entity AddElement(ECS::Entity parent, HM::Vector2 offset, HM::Vector2 size, const std::string& texture = {},
                               bool image = true)
        {
            const ECS::Entity entity = AddNode(parent);
            UiRectComponent   rect;
            rect.AnchorMin = rect.AnchorMax = rect.Pivot = HM::Vector2(0.0f, 0.0f);
            rect.Offset = offset;
            rect.Size   = size;
            ecs.AddComponent(entity, rect);
            if (image)
            {
                UiImageComponent component;
                component.Texture = texture;
                ecs.AddComponent(entity, component);
            }
            return entity;
        }

        HX::RenderScene Extract(HM::Vector2 target = TARGET)
        {
            HX::RenderScene scene;
            HX::SceneExtractor{}.ExtractUi(ecs, *uiSystem, target, scene);
            return scene;
        }
    };

    // Quad i's rect, from its top-left and bottom-right vertices.
    HX::UiRect QuadRect(const HX::RenderScene& scene, size_t i)
    {
        REQUIRE(scene.Ui.Vertices.size() >= (i + 1) * 4);
        const HX::UiVertex& topLeft     = scene.Ui.Vertices[i * 4];
        const HX::UiVertex& bottomRight = scene.Ui.Vertices[i * 4 + 2];
        return { topLeft.Position[0], topLeft.Position[1], bottomRight.Position[0] - topLeft.Position[0],
                 bottomRight.Position[1] - topLeft.Position[1] };
    }

    size_t QuadCount(const HX::RenderScene& scene) { return scene.Ui.Vertices.size() / 4; }
}

TEST_CASE("UI extraction - nested rects draw in hierarchy order, each inside its parent")
{
    UiFixture         fixture;
    const ECS::Entity canvas = fixture.AddCanvas();

    // A centred 200x100 panel, an icon 10 units into it, then a bar along the canvas's bottom.
    const ECS::Entity panel = fixture.AddNode(canvas);
    UiRectComponent   panelRect;
    panelRect.Size = HM::Vector2(200.0f, 100.0f);
    fixture.ecs.AddComponent(panel, panelRect);
    fixture.ecs.AddComponent(panel, UiImageComponent{});
    fixture.AddElement(panel, { 10.0f, 10.0f }, { 20.0f, 20.0f }, "Textures/icon.png");

    const ECS::Entity bar = fixture.AddNode(canvas);
    UiRectComponent   barRect;
    barRect.AnchorMin = HM::Vector2(0.0f, 1.0f);
    barRect.AnchorMax = HM::Vector2(1.0f, 1.0f);
    barRect.Pivot     = HM::Vector2(0.5f, 1.0f);
    barRect.Size      = HM::Vector2(0.0f, 40.0f);
    fixture.ecs.AddComponent(bar, barRect);
    fixture.ecs.AddComponent(bar, UiImageComponent{ "Textures/bar.png", HM::Vector4(1.0f, 0.0f, 0.0f, 1.0f) });

    const HX::RenderScene scene = fixture.Extract();
    CHECK(scene.UiTargetSize == TARGET);
    REQUIRE(QuadCount(scene) == 3);
    CHECK(QuadRect(scene, 0) == HX::UiRect{ 300.0f, 250.0f, 200.0f, 100.0f });
    CHECK(QuadRect(scene, 1) == HX::UiRect{ 310.0f, 260.0f, 20.0f, 20.0f });
    CHECK(QuadRect(scene, 2) == HX::UiRect{ 0.0f, 560.0f, 800.0f, 40.0f });

    // One command per texture change, in draw order; each texture numbered once.
    REQUIRE(scene.Ui.Commands.size() == 3);
    CHECK(scene.Ui.Commands[0].Texture == HX::UI_NO_TEXTURE);
    CHECK(scene.Ui.Commands[1].Texture == 0);
    CHECK(scene.Ui.Commands[2].Texture == 1);
    CHECK(scene.UiTextures == std::vector<std::string>{ "Textures/icon.png", "Textures/bar.png" });
    CHECK(scene.Ui.Commands[0].Scissor == HX::UiRect{ 0.0f, 0.0f, 800.0f, 600.0f });
    CHECK(scene.Ui.Vertices[8].Color == 0xFF0000FFu); // the bar's red
}

TEST_CASE("UI extraction - a reference-resolution canvas scales its layout to the target")
{
    UiFixture         fixture;
    const ECS::Entity canvas = fixture.AddCanvas();
    auto&             component = fixture.ecs.GetComponent<UiCanvasComponent>(canvas);
    component.ScaleMode           = UiCanvasScaleMode::ScaleWithTargetSize;
    component.ReferenceResolution = HM::Vector2(1600.0f, 1200.0f);
    fixture.AddElement(canvas, { 100.0f, 200.0f }, { 400.0f, 300.0f });

    const HX::RenderScene scene = fixture.Extract();
    REQUIRE(QuadCount(scene) == 1);
    CHECK(QuadRect(scene, 0) == HX::UiRect{ 50.0f, 100.0f, 200.0f, 150.0f });
}

TEST_CASE("UI extraction - hidden and editor-layer elements emit nothing, and neither do their children")
{
    UiFixture         fixture;
    const ECS::Entity canvas = fixture.AddCanvas();

    const ECS::Entity hidden = fixture.AddElement(canvas, { 0.0f, 0.0f }, { 50.0f, 50.0f });
    fixture.ecs.GetComponent<UiRectComponent>(hidden).IsVisible = false;
    fixture.AddElement(hidden, { 1.0f, 1.0f }, { 5.0f, 5.0f });

    const ECS::Entity editorOnly = fixture.AddElement(canvas, { 0.0f, 0.0f }, { 50.0f, 50.0f });
    RenderComponent   editorLayer;
    editorLayer.Layer = HX::EDITOR_LAYER;
    fixture.ecs.AddComponent(editorOnly, editorLayer);
    fixture.AddElement(editorOnly, { 1.0f, 1.0f }, { 5.0f, 5.0f });

    // A child that is no UI element hides its subtree; an element without an image draws only its children.
    const ECS::Entity plain = fixture.AddNode(canvas);
    fixture.AddElement(plain, { 1.0f, 1.0f }, { 5.0f, 5.0f });
    const ECS::Entity group = fixture.AddElement(canvas, { 100.0f, 100.0f }, { 50.0f, 50.0f }, {}, false);
    fixture.AddElement(group, { 5.0f, 5.0f }, { 10.0f, 10.0f });

    const HX::RenderScene scene = fixture.Extract();
    REQUIRE(QuadCount(scene) == 1);
    CHECK(QuadRect(scene, 0) == HX::UiRect{ 105.0f, 105.0f, 10.0f, 10.0f });

    // Showing the hidden element again brings back it and its child.
    fixture.ecs.GetComponent<UiRectComponent>(hidden).IsVisible = true;
    CHECK(QuadCount(fixture.Extract()) == 3);
}

TEST_CASE("UI extraction - disabled and editor-layer canvases emit nothing; canvases draw by sort order")
{
    UiFixture         fixture;
    const ECS::Entity late  = fixture.AddCanvas(5);
    fixture.AddElement(late, { 0.0f, 0.0f }, { 10.0f, 10.0f });
    const ECS::Entity early = fixture.AddCanvas(-1);
    fixture.AddElement(early, { 20.0f, 0.0f }, { 10.0f, 10.0f });
    const ECS::Entity tie = fixture.AddCanvas(5); // a later entity: after the first canvas of order 5
    fixture.AddElement(tie, { 40.0f, 0.0f }, { 10.0f, 10.0f });

    HX::RenderScene scene = fixture.Extract();
    REQUIRE(QuadCount(scene) == 3);
    CHECK(QuadRect(scene, 0).X == 20.0f);
    CHECK(QuadRect(scene, 1).X == 0.0f);
    CHECK(QuadRect(scene, 2).X == 40.0f);

    fixture.ecs.GetComponent<UiCanvasComponent>(early).IsEnabled = false;
    RenderComponent editorLayer;
    editorLayer.Layer = HX::EDITOR_LAYER;
    fixture.ecs.AddComponent(tie, editorLayer);
    scene = fixture.Extract();
    REQUIRE(QuadCount(scene) == 1);
    CHECK(QuadRect(scene, 0).X == 0.0f);

    // A canvas nested in another's tree draws only as its own root, once.
    fixture.ecs.GetComponent<ECS::HierarchyComponent>(late).Children.push_back(early);
    fixture.ecs.GetComponent<UiCanvasComponent>(early).IsEnabled = true;
    CHECK(QuadCount(fixture.Extract()) == 2);
}

TEST_CASE("UI extraction - a button tints its image by its state, and by DisabledTint when not interactable")
{
    UiFixture         fixture;
    const ECS::Entity canvas = fixture.AddCanvas();
    const ECS::Entity button = fixture.AddElement(canvas, { 0.0f, 0.0f }, { 10.0f, 10.0f });
    fixture.ecs.GetComponent<UiImageComponent>(button).Color = HM::Vector4(1.0f, 1.0f, 0.0f, 1.0f);

    UiButtonComponent component;
    component.NormalTint   = HM::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    component.HoverTint    = HM::Vector4(0.0f, 1.0f, 1.0f, 1.0f);
    component.PressedTint  = HM::Vector4(1.0f, 0.0f, 1.0f, 1.0f);
    component.DisabledTint = HM::Vector4(1.0f, 1.0f, 1.0f, 0.0f);
    fixture.ecs.AddComponent(button, component);

    const auto colorOf = [&]() { return fixture.Extract().Ui.Vertices[0].Color; };
    auto&      state   = fixture.ecs.GetComponent<UiButtonComponent>(button);
    CHECK(colorOf() == 0xFF00FFFFu); // yellow
    state.State = UiButtonState::Hovered;
    CHECK(colorOf() == 0xFF00FF00u); // green
    state.State = UiButtonState::Pressed;
    CHECK(colorOf() == 0xFF0000FFu); // red
    state.IsInteractable = false;
    CHECK(colorOf() == 0x0000FFFFu); // yellow, transparent
}

TEST_CASE("UI extraction - no target area draws nothing, and Clear empties the UI for the next frame")
{
    UiFixture         fixture;
    const ECS::Entity canvas = fixture.AddCanvas();
    fixture.AddElement(canvas, { 0.0f, 0.0f }, { 10.0f, 10.0f }, "Textures/a.png");

    HX::RenderScene scene = fixture.Extract({ 0.0f, 0.0f });
    CHECK(scene.Ui.Commands.empty());
    CHECK(scene.UiTextures.empty());

    HX::SceneExtractor{}.ExtractUi(fixture.ecs, *fixture.uiSystem, TARGET, scene);
    CHECK(QuadCount(scene) == 1);
    CHECK(scene.UiTextures.size() == 1);
    scene.Clear();
    CHECK(scene.Ui.Vertices.empty());
    CHECK(scene.Ui.Commands.empty());
    CHECK(scene.UiTextures.empty());
}
