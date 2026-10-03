#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"

#include "HedgehogInput/api/ActionState.hpp"
#include "HedgehogInput/api/DefaultInputActions.hpp"
#include "HedgehogInput/api/InputActionFile.hpp"

#include <optional>

using HedgehogEngine::EngineContext;

namespace
{
    size_t ActionIndex(const EngineContext& context, const char* name)
    {
        const std::optional<size_t> index = HInput::FindAction(context.GetInputActions().Game, name);
        REQUIRE(index.has_value());
        return *index;
    }

    HW::RawInput WithKey(HW::Key key)
    {
        HW::RawInput input;
        input.Keys[static_cast<size_t>(key)] = true;
        return input;
    }
}

TEST_CASE("Input actions - the engine loads the shipped actions file, which holds the defaults")
{
    EngineContext context;
    CHECK(context.GetInputActions() == HInput::MakeDefaultInputActions());

    const std::optional<std::string> text = context.GetFileSystem().ReadTextFile(EngineContext::INPUT_ACTIONS_PATH);
    REQUIRE(text.has_value());
    const HInput::InputActionParseResult parsed = HInput::ParseInputActions(*text);
    REQUIRE_MESSAGE(parsed.Actions.has_value(), parsed.Error);
    CHECK(*parsed.Actions == HInput::MakeDefaultInputActions());
}

TEST_CASE("Input actions - game actions are evaluated only while Playing")
{
    EngineContext      context;
    const size_t       submit = ActionIndex(context, "UiSubmit");
    const HW::RawInput enter  = WithKey(HW::Key::Enter);
    const HW::RawInput none;
    const auto&        state  = context.GetGameActionState();

    // Edit: nothing is ever down.
    context.UpdateGameInput(enter);
    CHECK_FALSE(HInput::IsActionDown(state, submit));

    REQUIRE(context.Play());
    context.UpdateGameInput(enter);
    CHECK(HInput::WasActionPressed(state, submit)); // held since Edit, pressed on Play's first frame
    context.UpdateGameInput(enter);
    CHECK(HInput::IsActionDown(state, submit));
    CHECK_FALSE(HInput::WasActionPressed(state, submit));
    context.UpdateGameInput(none);
    CHECK(HInput::WasActionReleased(state, submit));
    context.UpdateGameInput(none);
    CHECK_FALSE(HInput::WasActionReleased(state, submit));

    // Paused: up, whatever is held.
    context.UpdateGameInput(enter);
    REQUIRE(context.Pause());
    CHECK_FALSE(HInput::IsActionDown(state, submit));
    context.UpdateGameInput(enter);
    CHECK_FALSE(HInput::IsActionDown(state, submit));

    REQUIRE(context.Resume());
    context.UpdateGameInput(enter);
    CHECK(HInput::WasActionPressed(state, submit));

    // Stop leaves everything up.
    REQUIRE(context.Stop());
    CHECK_FALSE(HInput::IsActionDown(state, submit));
    context.UpdateGameInput(enter);
    CHECK_FALSE(HInput::IsActionDown(state, submit));
}

TEST_CASE("Input actions - a consumed action is up for that frame only")
{
    EngineContext context;
    const size_t  press = ActionIndex(context, "UiPointerPress");
    HW::RawInput  click;
    click.MouseButtons[static_cast<size_t>(HW::MouseButton::Left)] = true;

    REQUIRE(context.Play());
    context.UpdateGameInput(click);
    REQUIRE(HInput::WasActionPressed(context.GetGameActionState(), press));
    HInput::ConsumeAction(context.GetGameActionState(), press);
    CHECK_FALSE(HInput::WasActionPressed(context.GetGameActionState(), press));

    context.UpdateGameInput(click);
    CHECK(HInput::IsActionDown(context.GetGameActionState(), press));
    REQUIRE(context.Stop());
}
