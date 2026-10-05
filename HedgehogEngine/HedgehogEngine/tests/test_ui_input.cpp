#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"
#include "HedgehogEngine/api/ECS/components/UiButtonComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiCanvasComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiImageComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiRectComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/UiSystem.hpp"
#include "HedgehogEngine/api/Events/UiEvents.hpp"

#include "HedgehogEngine/api/Input/GameInputFrame.hpp"

#include "ECS/api/ECS.hpp"
#include "HedgehogInput/api/ActionState.hpp"

#include <optional>
#include <vector>

using namespace HedgehogEngine;

namespace
{
    const HM::Vector2 VIEW_SIZE = HM::Vector2(800.0f, 600.0f);

    // A Playing context with one constant-pixel-size canvas, recording every click.
    struct UiWorld
    {
        EngineContext            Context;
        ECS::Entity              Canvas = 0;
        std::vector<ECS::Entity> Clicks;

        UiWorld()
        {
            UiCanvasComponent canvas;
            canvas.ScaleMode = UiCanvasScaleMode::ConstantPixelSize;
            Canvas           = Context.GetSceneManager().CreateGameObject();
            Context.GetECS().AddComponent(Canvas, canvas);
            Context.GetEventBus().Subscribe<UiButtonClickedEvent>([this](const UiButtonClickedEvent& event)
                                                                   { Clicks.push_back(event.Entity); });
            REQUIRE(Context.Play());
        }

        // An element at (x, y) of width x height pixels under parent (the canvas by default).
        ECS::Entity Element(float x, float y, float width, float height, std::optional<ECS::Entity> parent = std::nullopt)
        {
            UiRectComponent rect;
            rect.AnchorMin = HM::Vector2(0.0f, 0.0f);
            rect.AnchorMax = HM::Vector2(0.0f, 0.0f);
            rect.Pivot     = HM::Vector2(0.0f, 0.0f);
            rect.Offset    = HM::Vector2(x, y);
            rect.Size      = HM::Vector2(width, height);
            const ECS::Entity entity = Context.GetSceneManager().CreateGameObject(parent.value_or(Canvas));
            Context.GetECS().AddComponent(entity, rect);
            return entity;
        }

        ECS::Entity Button(float x, float y, float width = 100.0f, float height = 50.0f, bool interactable = true)
        {
            const ECS::Entity entity = Element(x, y, width, height);
            UiButtonComponent button;
            button.IsInteractable = interactable;
            Context.GetECS().AddComponent(entity, button);
            Context.GetECS().AddComponent(entity, UiImageComponent{});
            return entity;
        }

        void Frame(const HW::RawInput& input) { Context.UpdateGameInput(input, VIEW_SIZE); }

        UiButtonState State(ECS::Entity button) { return Context.GetECS().GetComponent<UiButtonComponent>(button).State; }

        size_t Action(const char* name) const
        {
            const std::optional<size_t> index = HInput::FindAction(Context.GetInputActions().Game, name);
            REQUIRE(index.has_value());
            return *index;
        }
    };

    HW::RawInput Pointer(float x, float y, bool down = false)
    {
        HW::RawInput input;
        input.CursorPosition = HM::Vector2(x, y);
        input.CursorInside   = true;
        input.CursorKnown    = true;
        input.MouseButtons[static_cast<size_t>(HW::MouseButton::Left)] = down;
        return input;
    }

    // An Input-phase system registered after UiSystem: what the pointer press looks like to it.
    class PressWatcher : public ECS::System
    {
    public:
        explicit PressWatcher(size_t press)
            : m_Press(press)
        {
        }

        void OnRegister(ECS::ECS& ecs) override { m_Input = ecs.GetServices().Find<GameInputFrame>(); }
        ECS::SystemPhase GetPhase() const override { return ECS::SystemPhase::Input; }

        void OnFrame(ECS::ECS&, const ECS::FrameContext& ctx) override
        {
            ++Frames;
            Playing = ctx.Mode == ECS::PlayMode::Playing;
            Pressed = m_Input && m_Input->Actions && HInput::WasActionPressed(*m_Input->Actions, m_Press);
        }

        int  Frames  = 0;
        bool Playing = false;
        bool Pressed = false;

    private:
        size_t                m_Press = 0;
        const GameInputFrame* m_Input = nullptr;
    };

    HW::RawInput WithKey(HW::Key key)
    {
        HW::RawInput input;
        input.Keys[static_cast<size_t>(key)] = true;
        return input;
    }
}

TEST_CASE("UI input - a press and release on a button clicks it once, with hover and press states")
{
    UiWorld           world;
    const ECS::Entity button = world.Button(100.0f, 100.0f);

    world.Frame(Pointer(10.0f, 10.0f));
    CHECK(world.State(button) == UiButtonState::Normal);

    world.Frame(Pointer(150.0f, 120.0f));
    CHECK(world.State(button) == UiButtonState::Hovered);
    CHECK(world.Context.GetUiSystem()->GetHovered(world.Context.GetECS()) == button);

    world.Frame(Pointer(150.0f, 120.0f, true));
    CHECK(world.State(button) == UiButtonState::Pressed);
    CHECK(world.Clicks.empty());

    world.Frame(Pointer(150.0f, 120.0f, true));
    CHECK(world.State(button) == UiButtonState::Pressed);
    CHECK(world.Clicks.empty());

    world.Frame(Pointer(150.0f, 120.0f));
    REQUIRE(world.Clicks.size() == 1);
    CHECK(world.Clicks[0] == button);
    CHECK(world.State(button) == UiButtonState::Hovered); // hovered, and focused by the click
    CHECK(world.Context.GetUiSystem()->GetFocused(world.Context.GetECS()) == button);

    world.Frame(Pointer(150.0f, 120.0f));
    CHECK(world.Clicks.size() == 1);
}

TEST_CASE("UI input - a press that ends off its button, or begins off it, clicks nothing")
{
    UiWorld           world;
    const ECS::Entity a = world.Button(0.0f, 0.0f);
    const ECS::Entity b = world.Button(200.0f, 0.0f);

    // Pressed on a, dragged to b: a is shown normal while the pointer is away, nothing clicks.
    world.Frame(Pointer(50.0f, 25.0f, true));
    CHECK(world.State(a) == UiButtonState::Pressed);
    world.Frame(Pointer(250.0f, 25.0f, true));
    CHECK(world.State(b) == UiButtonState::Normal); // hovered, but another button holds the press
    world.Frame(Pointer(250.0f, 25.0f));
    CHECK(world.Clicks.empty());

    // Pressed on empty space, released on a.
    world.Frame(Pointer(500.0f, 500.0f, true));
    world.Frame(Pointer(50.0f, 25.0f, true));
    world.Frame(Pointer(50.0f, 25.0f));
    CHECK(world.Clicks.empty());
}

TEST_CASE("UI input - hit testing follows draw order, visibility and IsInteractable")
{
    UiWorld           world;
    const ECS::Entity under    = world.Button(0.0f, 0.0f, 200.0f, 200.0f);
    const ECS::Entity over     = world.Button(50.0f, 50.0f);
    const ECS::Entity disabled = world.Button(0.0f, 300.0f, 200.0f, 200.0f, false);
    const ECS::Entity beneath  = world.Button(300.0f, 0.0f);
    const ECS::Entity image    = world.Element(300.0f, 0.0f, 100.0f, 100.0f); // drawn after beneath, so on top
    world.Context.GetECS().AddComponent(image, UiImageComponent{});
    const ECS::Entity hidden = world.Button(0.0f, 400.0f, 50.0f, 50.0f); // over the disabled one, but hidden
    world.Context.GetECS().GetComponent<UiRectComponent>(hidden).IsVisible = false;

    const auto hovered = [&](float x, float y)
    {
        world.Frame(Pointer(x, y));
        return world.Context.GetUiSystem()->GetHovered(world.Context.GetECS());
    };
    CHECK(hovered(100.0f, 75.0f) == over);
    CHECK(hovered(10.0f, 10.0f) == under);
    CHECK_FALSE(hovered(100.0f, 350.0f).has_value());  // not interactable
    CHECK_FALSE(hovered(310.0f, 10.0f).has_value());   // the image above blocks it
    CHECK_FALSE(hovered(10.0f, 410.0f).has_value());   // the hidden one takes nothing
    CHECK(world.State(beneath) == UiButtonState::Normal);
    CHECK(world.State(disabled) == UiButtonState::Normal);

    // A child drawn after its parent is above it.
    const ECS::Entity child = world.Element(0.0f, 0.0f, 20.0f, 20.0f, over);
    world.Context.GetECS().AddComponent(child, UiButtonComponent{});
    CHECK(hovered(55.0f, 55.0f) == child);
}

TEST_CASE("UI input - navigation moves focus and submit clicks the focused button")
{
    UiWorld           world;
    const ECS::Entity left   = world.Button(0.0f, 0.0f);
    const ECS::Entity middle = world.Button(120.0f, 0.0f);
    const ECS::Entity right  = world.Button(240.0f, 0.0f);
    const ECS::Entity below  = world.Button(120.0f, 100.0f);
    const auto& state = world.Context.GetGameActionState();
    const HW::RawInput none;

    world.Frame(WithKey(HW::Key::Right)); // nothing focused: the first button takes focus
    CHECK(world.Context.GetUiSystem()->GetFocused(world.Context.GetECS()) == left);
    CHECK(world.State(left) == UiButtonState::Hovered);
    CHECK_FALSE(HInput::WasActionPressed(state, world.Action("UiNavigateRight"))); // consumed
    world.Frame(none);

    world.Frame(WithKey(HW::Key::Right));
    world.Frame(none);
    CHECK(world.Context.GetUiSystem()->GetFocused(world.Context.GetECS()) == middle);
    CHECK(world.State(left) == UiButtonState::Normal);

    world.Frame(WithKey(HW::Key::Down));
    world.Frame(none);
    CHECK(world.Context.GetUiSystem()->GetFocused(world.Context.GetECS()) == below);
    world.Frame(WithKey(HW::Key::Down)); // nothing further down: focus stays
    world.Frame(none);
    CHECK(world.Context.GetUiSystem()->GetFocused(world.Context.GetECS()) == below);

    world.Frame(WithKey(HW::Key::Enter));
    REQUIRE(world.Clicks.size() == 1);
    CHECK(world.Clicks[0] == below);
    CHECK(world.State(below) == UiButtonState::Pressed);
    CHECK_FALSE(HInput::WasActionPressed(state, world.Action("UiSubmit")));
    world.Frame(WithKey(HW::Key::Enter)); // held: no second click
    CHECK(world.Clicks.size() == 1);
    world.Frame(none);
    CHECK(world.State(below) == UiButtonState::Hovered);
    CHECK(world.State(right) == UiButtonState::Normal);

    // A button that stops being interactable loses focus.
    world.Context.GetECS().GetComponent<UiButtonComponent>(below).IsInteractable = false;
    world.Frame(none);
    CHECK_FALSE(world.Context.GetUiSystem()->GetFocused(world.Context.GetECS()).has_value());
    CHECK(world.State(below) == UiButtonState::Normal);
}

TEST_CASE("UI input - without buttons, navigation and submit reach the game")
{
    UiWorld world;
    world.Frame(WithKey(HW::Key::Right));
    CHECK(HInput::WasActionPressed(world.Context.GetGameActionState(), world.Action("UiNavigateRight")));
    world.Frame(WithKey(HW::Key::Enter));
    CHECK(HInput::WasActionPressed(world.Context.GetGameActionState(), world.Action("UiSubmit")));
}

TEST_CASE("UI input - the pointer's actions are consumed only over the UI")
{
    UiWorld           world;
    const ECS::Entity button = world.Button(100.0f, 100.0f);
    const auto&       state  = world.Context.GetGameActionState();
    const size_t      press  = world.Action("UiPointerPress");

    world.Frame(Pointer(500.0f, 500.0f, true));
    CHECK(HInput::WasActionPressed(state, press));
    world.Frame(Pointer(500.0f, 500.0f));

    world.Frame(Pointer(150.0f, 120.0f, true));
    CHECK_FALSE(HInput::WasActionPressed(state, press));
    CHECK(world.State(button) == UiButtonState::Pressed);
    world.Frame(Pointer(500.0f, 500.0f, true)); // dragged off: the press still belongs to the UI
    CHECK_FALSE(HInput::IsActionDown(state, press));
    world.Frame(Pointer(500.0f, 500.0f));
    CHECK(world.Clicks.empty());
}

TEST_CASE("UI input - nothing happens outside Play, and states reset when Play ends")
{
    UiWorld           world;
    const ECS::Entity button = world.Button(100.0f, 100.0f);

    world.Frame(Pointer(150.0f, 120.0f, true));
    CHECK(world.State(button) == UiButtonState::Pressed);

    REQUIRE(world.Context.Pause());
    world.Frame(Pointer(150.0f, 120.0f));
    CHECK(world.State(button) == UiButtonState::Normal);
    CHECK(world.Clicks.empty());

    // Stop restores the scene from before Play, which had no button: make one in Edit.
    REQUIRE(world.Context.Stop());
    const ECS::Entity edited = world.Button(100.0f, 100.0f);
    world.Frame(Pointer(150.0f, 120.0f, true));
    world.Frame(Pointer(150.0f, 120.0f));
    CHECK(world.Clicks.empty());
    CHECK(world.State(edited) == UiButtonState::Normal);
    CHECK_FALSE(world.Context.GetUiSystem()->GetHovered(world.Context.GetECS()).has_value());

    // A zero view size gives the UI no input even while Playing.
    REQUIRE(world.Context.Play());
    world.Context.UpdateGameInput(Pointer(150.0f, 120.0f, true));
    CHECK(world.State(edited) == UiButtonState::Normal);
    world.Context.UpdateGameInput(Pointer(150.0f, 120.0f));
    CHECK(world.Clicks.empty());
}

TEST_CASE("UI input - UiSystem runs in the Input phase, before later Input systems see the actions")
{
    UiWorld           world;
    const ECS::Entity button = world.Button(100.0f, 100.0f);
    CHECK(world.Context.GetUiSystem()->GetPhase() == ECS::SystemPhase::Input);
    auto watcher = world.Context.GetECS().RegisterSystem<PressWatcher>(world.Action("UiPointerPress"));

    world.Frame(Pointer(500.0f, 500.0f, true)); // off the UI: the press reaches the watcher
    CHECK(watcher->Frames == 1);
    CHECK(watcher->Playing);
    CHECK(watcher->Pressed);
    world.Frame(Pointer(500.0f, 500.0f));

    world.Frame(Pointer(150.0f, 120.0f, true)); // over the button: the UI consumed it first
    CHECK(world.State(button) == UiButtonState::Pressed);
    CHECK_FALSE(watcher->Pressed);

    // UpdateContext does not run the Input phase.
    world.Context.UpdateContext(1.0f, 1.0f / 60.0f);
    CHECK(watcher->Frames == 3);
}
